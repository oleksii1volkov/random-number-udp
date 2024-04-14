#include "protocol.pb.h"
#include "server/config.hpp"
#include "utils/options.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

#include <concepts>
#include <limits>
#include <print>
#include <random>
#include <thread>

using boost::asio::as_tuple_t;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using boost::asio::ip::udp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using udp_socket = default_token::as_default_on_t<udp::socket>;
namespace this_coro = boost::asio::this_coro;

template <typename... Args>
void log(std::format_string<Args...> format, Args &&...args) {
    std::println(std::move(format), std::forward<Args>(args)...);
}

template <std::integral NumberType>
void add_random_numbers(auto numbers, size_t number_count) {
    std::random_device device;
    std::mt19937 generator{device()};
    std::uniform_int_distribution<NumberType> distribution{
        std::numeric_limits<NumberType>::min(),
        std::numeric_limits<NumberType>::max()};

    while (--number_count) {
        numbers->Add(distribution(generator));
    }
}

class NumberGeneratorServer {
public:
    NumberGeneratorServer(boost::asio::io_context &io_context, uint16_t port)
        : io_context_{io_context},
          socket_{io_context, udp::endpoint{udp::v4(), port}},
          buffer_(DATAGRAM_SIZE, '\0') {
        socket_.set_option(boost::asio::socket_base::reuse_address(true));
    }

    NumberGeneratorServer(const NumberGeneratorServer &) = delete;
    NumberGeneratorServer &operator=(const NumberGeneratorServer &) = delete;
    NumberGeneratorServer(NumberGeneratorServer &&) = delete;
    NumberGeneratorServer &operator=(NumberGeneratorServer &&) = delete;

    void start() {
        co_spawn(
            io_context_,
            [this]() -> boost::asio::awaitable<void> {
                for (;;) {
                    const auto request = co_await accept_request();
                    const auto response = create_response(request);
                    co_await send_response(response);
                }
            },
            detached);
    }

private:
    uint64_t
    get_max_datagram_number_count(const protocol::NumberResponse &response) {
        return (DATAGRAM_SIZE - response.ByteSizeLong()) / sizeof(uint64_t);
    }

    uint64_t get_datagram_count(const protocol::NumberResponse &response) {
        const auto number_count = response.number_count();
        const auto max_datagram_number_count =
            get_max_datagram_number_count(response);

        const auto datagram_count =
            (number_count / max_datagram_number_count) +
            ((number_count % max_datagram_number_count) != 0 ? 1 : 0);

        return datagram_count;
    }

    std::optional<uint64_t>
    get_datagram_number_count(const protocol::NumberResponse &response) {
        const auto datagram_count = get_datagram_count(response);
        if (response.number_sequence() >= datagram_count) {
            return std::nullopt;
        }

        const auto number_count = response.number_count();
        const auto number_sequence = response.number_sequence();
        auto datagram_number_count = get_max_datagram_number_count(response);

        if (number_sequence == (datagram_count - 1)) {
            datagram_number_count = number_count % datagram_number_count;
        }

        return datagram_number_count;
    }

    protocol::NumberResponse
    create_response(const protocol::NumberRequest &request) {
        protocol::NumberResponse response;
        response.set_protocol_version(PROTOCOL_VERSION);

        auto error = protocol::Error::OK;
        std::string error_message;

        if (request.protocol_version() < PROTOCOL_VERSION) {
            error = protocol::Error::CLIENT_TOO_OLD;
            error_message = std::format(
                "Client is too old. Minimum supported version is {}",
                PROTOCOL_VERSION);
        } else if (request.protocol_version() > PROTOCOL_VERSION) {
            error = protocol::Error::CLIENT_TOO_NEW;
            error_message = std::format(
                "Client is too new. Maximum supported version is {}",
                PROTOCOL_VERSION);
        }

        if (error != protocol::Error::OK) {
            response.set_error(error);
            response.set_error_message(error_message);
            return response;
        }

        response.set_number_count(request.number_count());
        response.set_number_sequence(request.number_sequence());
        const auto datagram_count = get_datagram_count(response);

        if (request.number_sequence() >= datagram_count) {
            error = protocol::Error::SEQUENCE_NUMBER_TOO_HIGH;
            error_message = std::format(
                "Sequence number {} is too high. Maximum sequence number is {}",
                request.number_sequence(), datagram_count - 1);
        }

        if (error != protocol::Error::OK) {
            response.set_error(error);
            response.set_error_message(error_message);
            return response;
        }

        const auto datagram_number_count = get_datagram_number_count(response);
        assert(datagram_number_count);
        auto numbers = response.mutable_numbers();
        numbers->Reserve(*datagram_number_count);
        add_random_numbers<uint64_t>(numbers, *datagram_number_count);

        return response;
    }

    awaitable<protocol::NumberRequest> accept_request() {
        try {
            protocol::NumberRequest request;

            auto [request_error, request_length] =
                co_await socket_.async_receive_from(
                    boost::asio::buffer(buffer_.data(), buffer_.size()),
                    sender_endpoint_);

            if (request_error) {
                log("Error while receiving request\nRequest: \nError: {}",
                    request_error.message());
                co_return request;
            }

            if (request_length == 0) {
                log("Error while receiving request\nRequest: ");
                co_return request;
            }

            buffer_.resize(request_length);
            request.ParseFromString(buffer_);
            log("Received request from {}\nRequest: ",
                sender_endpoint_.address().to_v4().to_string());

            co_return request;
        } catch (std::exception &error) {
            log("Exception: {}", error.what());
        }
    }

    awaitable<void> send_response(const protocol::NumberResponse &response) {
        buffer_.clear();
        response.SerializeToString(&buffer_);

        auto [response_error, response_length] = co_await socket_.async_send_to(
            boost::asio::buffer(buffer_.data(), buffer_.size()),
            sender_endpoint_);

        if (response_error) {
            log("Error while sending response\nResponse: \nError: {}",
                response_error.message());
            co_return;
        }

        if (response_length == 0) {
            log("Error while sending response\nResponse: ");
            co_return;
        }
    }

private:
    static constexpr uint32_t PROTOCOL_VERSION{1};
    static constexpr uint32_t DATAGRAM_SIZE{508};

    boost::asio::io_context &io_context_;
    udp_socket socket_;
    udp::endpoint sender_endpoint_;
    std::string buffer_;
};

int main(int argc, char *argv[]) {
    try {
        const auto command_line_options =
            utils::CommandLineOptions::extract(argc, argv);

        server::Config config{command_line_options.config_path()};

        boost::asio::io_context io_context;

        boost::asio::signal_set signals{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        log("Starting server on port: {}", config.port());
        NumberGeneratorServer server{io_context, config.port()};
        server.start();
        // co_spawn(io_context, handshake(), detached);

        io_context.run();
    } catch (std::exception &error) {
        log("Exception: {}\n", error.what());
    }
}
