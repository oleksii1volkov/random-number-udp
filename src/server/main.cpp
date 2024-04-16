#include "constants.hpp"
#include "protocol.pb.h"
#include "server/config.hpp"
#include "utils/checksum.hpp"
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
using namespace protocol;

class UDPRandomGeneratorServer {
public:
    UDPRandomGeneratorServer(boost::asio::io_context &io_context,
                             const server::Config &config,
                             utils::Logger &logger)
        : io_context_{io_context},
          socket_{io_context, udp::endpoint{udp::v4(), config.port()}},
          buffer_(MESSAGE_MAX_SIZE, '\0'), logger_{logger} {
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
        decltype(std::declval<NumberSequenceResponse>().numbers().Get(0))>;

    awaitable<void> run() {
        using namespace protocol;

        for (;;) {
            try {
                const auto version_request =
                    co_await receive_request<ProtocolVersionRequest>();

                const auto version_response =
                    create_protocol_version_response(version_request);
                co_await send_response(version_response);

                if (version_response.error() !=
                    ProtocolVersionError::VERSION_OK) {
                    continue;
                }

                const auto number_request =
                    co_await receive_request<NumberSequenceRequest>();

                const auto sequence_count =
                    get_sequence_count(number_request.number_count());

                for (uint64_t sequence_index{0};
                     sequence_index < sequence_count; ++sequence_index) {
                    co_await send_number_sequence_response(
                        number_request, sequence_index, sequence_count);
                }
            } catch (std::exception &error) {
                logger_.log("Exception: {}", error.what());
            }
        }
    }

    awaitable<void>
    send_number_sequence_response(const NumberSequenceRequest &sequence_request,
                                  uint64_t sequence_index,
                                  uint64_t sequence_count) {
        const auto sequence_response = create_number_sequence_response(
            sequence_request, sequence_index, sequence_count);

        for (uint8_t retry_index{0};
             retry_index <= SEQUENCE_RESPONSE_MAX_RETRIES_COUNT;
             ++retry_index) {
            co_await send_response(sequence_response);

            const auto ack_request =
                co_await receive_request<NumberSequenceAckRequest>();

            if (ack_request.ack() == NumberSequenceAck::ACK_OK) {
                co_return;
            } else {
                logger_.log(
                    "Failed to acknowledge number sequence {}. Expected "
                    "checksum: {}. Actual checksum: {}. Retry: {}",
                    sequence_response.sequence_index(),
                    sequence_response.checksum(), ack_request.checksum(),
                    retry_index);
            }
        }
    }

    template <typename RequestType> awaitable<RequestType> receive_request() {
        buffer_.resize(buffer_.capacity());
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
        RequestType request;
        request.ParseFromString(buffer_);

        logger_.log("Received request from {}\nRequest: {}",
                    sender_endpoint_.address().to_v4().to_string(), request);

        co_return request;
    }

    template <typename ResponseType>
    awaitable<void> send_response(const ResponseType &response) {
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

    ProtocolVersionResponse
    create_protocol_version_response(const ProtocolVersionRequest &request) {
        ProtocolVersionResponse response;
        response.set_protocol_version(PROTOCOL_VERSION);
        response.set_error(ProtocolVersionError::VERSION_OK);

        if (request.protocol_version() < PROTOCOL_VERSION) {
            response.set_error(ProtocolVersionError::CLIENT_TOO_OLD);
        } else if (request.protocol_version() > PROTOCOL_VERSION) {
            response.set_error(ProtocolVersionError::CLIENT_TOO_NEW);
        }

        switch (response.error()) {
        case ProtocolVersionError::VERSION_OK:
            break;
        case ProtocolVersionError::CLIENT_TOO_OLD:
            response.set_error_message(std::format(
                "Client is too old. Minimum supported version is {}",
                PROTOCOL_VERSION));
            break;
        case ProtocolVersionError::CLIENT_TOO_NEW:
            response.set_error_message(std::format(
                "Client is too new. Maximum supported version is {}",
                PROTOCOL_VERSION));
            break;
        default:
            std::unreachable();
        }

        return response;
    }

    NumberSequenceResponse
    create_number_sequence_response(const NumberSequenceRequest &request,
                                    uint64_t sequence_index,
                                    uint64_t sequence_count) {
        NumberSequenceResponse response;

        response.set_number_count(request.number_count());
        response.set_upper_bound(request.upper_bound());
        response.set_sequence_index(sequence_index);
        response.set_sequence_count(sequence_count);

        auto sequence_number_count = get_sequence_max_number_count();
        if (sequence_index == (sequence_count - 1)) {
            sequence_number_count =
                request.number_count() % sequence_number_count;
        }

        response.set_sequence_number_count(sequence_number_count);

        if (request.upper_bound() < 0) {
            response.set_error(NumberSequenceError::INVALID_UPPER_BOUND);
            response.set_error_message("Upper bound must be greater than zero");

            return response;
        }

        response.mutable_numbers()->Reserve(sequence_number_count);

        add_random_numbers(response);
        response.set_checksum(utils::calculate_checksum(response.numbers()));

        return response;
    }

    uint64_t get_sequence_max_number_count() {
        NumberSequenceResponse response;
        response.set_number_count(0);
        response.set_upper_bound(0);
        response.set_sequence_index(0);
        response.set_sequence_count(0);
        response.set_sequence_number_count(0);
        response.set_checksum(0);
        response.clear_error();
        response.clear_error_message();

        const size_t number_type_size = sizeof(NumberType);
        const size_t overhead_per_number = 2;

        return ((MESSAGE_MAX_SIZE - response.ByteSizeLong()) /
                (number_type_size + overhead_per_number));
    }

    uint64_t get_sequence_count(uint64_t number_count) {
        const auto sequence_max_number_count = get_sequence_max_number_count();

        auto sequence_count = (number_count / sequence_max_number_count);
        if ((number_count % sequence_max_number_count) != 0) {
            ++sequence_count;
        }

        return sequence_count;
    }

    void add_random_numbers(NumberSequenceResponse &response) {
        auto number_count = response.sequence_number_count();
        const auto upper_bound = response.upper_bound();

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
                response.mutable_numbers()->Add(number);
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
