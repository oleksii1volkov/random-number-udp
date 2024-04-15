#include "protocol.pb.h"
#include "server/config.hpp"
#include "utils/formatters.hpp"
#include "utils/logger.hpp"
#include "utils/options.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

#include <atomic>
#include <concepts>
#include <limits>
#include <print>
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using boost::asio::as_tuple_t;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using boost::asio::ip::udp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using udp_socket = default_token::as_default_on_t<udp::socket>;
namespace this_coro = boost::asio::this_coro;

class UDPRandomGeneratorServer {
public:
    UDPRandomGeneratorServer(boost::asio::io_context &io_context,
                             const server::Config &config,
                             utils::Logger &logger)
        : io_context_{io_context},
          socket_{io_context, udp::endpoint{udp::v4(), config.port()}},
          buffer_(DATAGRAM_SIZE, '\0'), logger_{logger} {
        socket_.set_option(boost::asio::socket_base::reuse_address(true));
    }

    UDPRandomGeneratorServer(const UDPRandomGeneratorServer &) = delete;
    UDPRandomGeneratorServer &
    operator=(const UDPRandomGeneratorServer &) = delete;
    UDPRandomGeneratorServer(UDPRandomGeneratorServer &&) = delete;
    UDPRandomGeneratorServer &operator=(UDPRandomGeneratorServer &&) = delete;

    void start() {
        co_spawn(
            io_context_,
            [this]() -> boost::asio::awaitable<void> { co_await run(); },
            detached);
    }

private:
    using NumberType = std::remove_cvref_t<
        decltype(std::declval<protocol::NumberResponse>().numbers().Get(0))>;

    awaitable<void> run() {
        for (;;) {
            try {
                const auto request = co_await accept_request();

                if (const auto result = validate_request(request);
                    result == protocol::Error::OK) {
                    init_sender(request.number_count());
                    const auto datagram_count =
                        get_datagram_count(request.number_count());

                    for (uint64_t datagram_index{0};
                         datagram_index < datagram_count; ++datagram_index) {
                        auto response =
                            create_response(request.number_count(),
                                            datagram_index, datagram_count);

                        process_request(request, response);
                        co_await send_response(response);
                    }

                    free_sender();
                } else {
                    auto response =
                        create_response(request.number_count(), 0, 0);
                    set_error(response, result);
                    co_await send_response(response);
                }
            } catch (std::exception &error) {
                logger_.log("Exception: {}", error.what());
            }
        }
    }

    awaitable<protocol::NumberRequest> accept_request() {
        protocol::NumberRequest request;

        const auto [request_error, request_length] =
            co_await socket_.async_receive_from(
                boost::asio::buffer(buffer_.data(), buffer_.size()),
                sender_endpoint_);

        if (request_error) {
            throw std::runtime_error{
                std::format("Failed to receive request\nError: {}",
                            request_error.message())};
        }

        if (request_length == 0) {
            throw std::runtime_error{
                "Failed to receive request\nError: no bytes received"};
        }

        buffer_.resize(request_length);
        request.ParseFromString(buffer_);
        logger_.log("Received request from {}\nRequest: {}",
                    sender_endpoint_.address().to_v4().to_string(), request);

        co_return request;
    }

    awaitable<void> send_response(const protocol::NumberResponse &response) {
        logger_.log("Sending response to {}\nResponse: {}",
                    sender_endpoint_.address().to_v4().to_string(), response);

        buffer_.clear();
        response.SerializeToString(&buffer_);

        auto [response_error, response_length] = co_await socket_.async_send_to(
            boost::asio::buffer(buffer_.data(), buffer_.size()),
            sender_endpoint_);

        if (response_error) {
            throw std::runtime_error{
                std::format("Failed to send response\nError: {}",
                            response_error.message())};
        }

        if (response_length == 0) {
            throw std::runtime_error{
                std::format("Failed to send response\nError: no bytes sent")};
        }
    }

    void process_request(const protocol::NumberRequest &request,
                         protocol::NumberResponse &response) {
        auto numbers = response.mutable_numbers();
        const auto datagram_number_count = response.datagram_number_count();
        numbers->Reserve(datagram_number_count);

        add_random_numbers(numbers, datagram_number_count,
                           request.upper_bound());
    }

    protocol::Error validate_request(const protocol::NumberRequest &request) {
        if (request.protocol_version() < PROTOCOL_VERSION) {
            return protocol::Error::CLIENT_TOO_OLD;
        } else if (request.protocol_version() > PROTOCOL_VERSION) {
            return protocol::Error::CLIENT_TOO_NEW;
        }

        if (request.upper_bound() < 0) {
            return protocol::Error::INVALID_UPPER_BOUND;
        }

        return protocol::Error::OK;
    }

    protocol::NumberResponse create_response(uint64_t number_count,
                                             uint64_t datagram_index,
                                             uint64_t datagram_count) {
        protocol::NumberResponse response;
        response.set_protocol_version(PROTOCOL_VERSION);
        response.set_number_count(number_count);
        response.set_datagram_index(datagram_index);
        response.set_datagram_count(datagram_count);

        auto datagram_number_count = get_max_datagram_number_count();
        if (datagram_index == (datagram_count - 1)) {
            datagram_number_count = number_count % datagram_number_count;
        }

        response.set_datagram_number_count(datagram_number_count);

        return response;
    }

    void set_error(protocol::NumberResponse &response, protocol::Error error) {
        response.set_error(error);

        switch (error) {
        case protocol::Error::OK:
            break;
        case protocol::Error::CLIENT_TOO_OLD:
            response.set_error_message(std::format(
                "Client is too old. Minimum supported version is {}",
                PROTOCOL_VERSION));
            break;
        case protocol::Error::CLIENT_TOO_NEW:
            response.set_error_message(std::format(
                "Client is too new. Maximum supported version is {}",
                PROTOCOL_VERSION));
            break;
        case protocol::Error::INVALID_UPPER_BOUND:
            response.set_error_message(
                "Invalid upper bound. Value should be greater than zero");
        default:
            std::unreachable();
        }
    }

    uint64_t get_max_datagram_number_count() {
        return ((DATAGRAM_SIZE - protocol::NumberResponse{}.ByteSizeLong()) /
                sizeof(NumberType)) -
               1;
    }

    uint64_t get_datagram_count(uint64_t number_count) {
        const auto max_datagram_number_count = get_max_datagram_number_count();

        auto datagram_count = (number_count / max_datagram_number_count);
        if ((number_count % max_datagram_number_count) != 0) {
            ++datagram_count;
        }

        return datagram_count;
    }

    void add_random_numbers(auto numbers, size_t number_count,
                            double upper_bound) {
        std::random_device device;
        std::mt19937 generator{device()};
        std::uniform_real_distribution<double> distribution{-upper_bound,
                                                            upper_bound};

        auto &client_numbers = senders_[sender_endpoint_];
        const size_t retries_count{10};

        while (number_count--) {
            auto number = distribution(generator);

            for (size_t retry_index{0};
                 retry_index < retries_count && client_numbers.contains(number);
                 ++retry_index) {
                number = distribution(generator);
            }

            if (client_numbers.contains(number)) {
                throw std::runtime_error{
                    std::format("Failed to generate unique number. "
                                "Maximum retries exceeded")};
            } else {
                numbers->Add(number);
                client_numbers.insert(number);
            }
        }
    }

    void init_sender(uint64_t number_count) {
        auto &numbers = senders_[sender_endpoint_];
        numbers.reserve(number_count);
    }

    void free_sender() { senders_.erase(sender_endpoint_); }

private:
    static constexpr uint32_t PROTOCOL_VERSION{1};
    static constexpr uint32_t DATAGRAM_SIZE{508};

    boost::asio::io_context &io_context_;
    udp_socket socket_;
    udp::endpoint sender_endpoint_;
    std::string buffer_;
    utils::Logger &logger_;
    std::unordered_map<udp::endpoint, std::unordered_set<NumberType>> senders_;
};

int main(int argc, char *argv[]) {
    try {
        utils::CommandLineOptions command_line_options;
        command_line_options.parse(argc, argv);
        server::Config config{command_line_options.config_path()};
        utils::Logger logger{command_line_options.logs_path()};

        boost::asio::io_context io_context;
        auto work = boost::asio::make_work_guard(io_context);
        boost::asio::signal_set signals{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto) {
            io_context.stop();
            work.reset();
        });

        logger.log("Starting server on port: {}", config.port());
        UDPRandomGeneratorServer server{io_context, config, logger};
        server.start();

        std::size_t thread_count = std::thread::hardware_concurrency();
        std::vector<std::jthread> threads;
        threads.reserve(thread_count);

        logger.log("Launching {} threads", thread_count);

        for (std::size_t thread_index{0}; thread_index < thread_count;
             ++thread_index) {
            threads.emplace_back([&] { io_context.run(); });
        }

        io_context.run();
    } catch (std::exception &error) {
        std::println(std::cerr, "Exception: {}\n", error.what());
        return 1;
    }

    return 0;
}
