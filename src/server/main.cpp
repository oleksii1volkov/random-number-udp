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

#include "protocol.pb.h"

using boost::asio::as_tuple_t;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using boost::asio::ip::udp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using udp_socket = default_token::as_default_on_t<udp::socket>;
namespace this_coro = boost::asio::this_coro;

inline constexpr uint32_t PROTOCOL_VERSION{1};
inline constexpr uint32_t DATAGRAM_SIZE{508};

template <typename... Args>
void log(std::format_string<Args...> format, Args &&...args) {
    std::println(std::move(format), std::forward<Args>(args)...);
}

template <typename ContainerType, typename ValueType>
concept PushBackContainer = requires(ContainerType c, ValueType v) {
    { c.clear() } -> std::same_as<void>;
    { c.reserve(size_t{}) } -> std::same_as<void>;
    { c.push_back(v) } -> std::same_as<void>;
};

template <std::integral NumberType, PushBackContainer<NumberType> ContainerType>
void generate_random_numbers(size_t number_count, ContainerType &numbers) {
    if (number_count == 0) {
        return;
    }

    std::random_device device;
    std::mt19937 generator{device()};
    std::uniform_int_distribution<> distribution{
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int32_t>::max()};

    numbers.clear();
    numbers.reserve(number_count);

    while (--number_count) {
        numbers.push_back(distribution(generator));
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
    protocol::NumberResponse
    create_response(const protocol::NumberRequest &request) {
        protocol::NumberResponse response;
        response.set_protocol_verison(PROTOCOL_VERSION);

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
        }

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
    boost::asio::io_context &io_context_;
    udp_socket socket_;
    udp::endpoint sender_endpoint_;
    std::string buffer_;
};

int main() {
    try {
        boost::asio::io_context io_context{1};

        boost::asio::signal_set signals{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) { io_context.stop(); });

        NumberGeneratorServer server{io_context, 55555};
        server.start();
        // co_spawn(io_context, handshake(), detached);

        io_context.run();
    } catch (std::exception &error) {
        log("Exception: {}\n", error.what());
    }
}
