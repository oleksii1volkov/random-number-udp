#include "client/config.hpp"
#include "client/options.hpp"
#include "protocol.pb.h"
#include "utils/formatters.hpp"
#include "utils/logger.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

#include <algorithm>
#include <execution>
#include <filesystem>
#include <fstream>
#include <print>

using boost::asio::as_tuple_t;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using boost::asio::ip::udp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using udp_socket = default_token::as_default_on_t<udp::socket>;

class UDPNumberSorterClient {
public:
    UDPNumberSorterClient(boost::asio::io_context &io_context,
                          const client::Config &config,
                          const std::filesystem::path &numbers_file_path,
                          utils::Logger &logger)
        : io_context_{io_context},
          socket_{io_context, udp::endpoint{udp::v4(), 0}},
          buffer_(DATAGRAM_SIZE, '\0'), config_{config},
          numbers_file_path_{numbers_file_path}, logger_{logger} {

        udp::resolver resolver{io_context};
        endpoint_ = *resolver
                         .resolve(udp::v4(), config.host(),
                                  std::to_string(config.port()))
                         .begin();
    }

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
        try {
            const auto request = create_request();
            co_await send_request(request);
            auto response = co_await receive_response();

            if (const auto result = validate_response(response);
                result == protocol::Error::OK) {
                init_numbers(response);
                process_response(response);

                while (response.datagram_index() !=
                       (response.datagram_count() - 1)) {
                    response = co_await receive_response();
                    process_response(response);
                }

                merge_numbers();
                flush_numbers();
            } else {
                logger_.log("Response validation failed. Error: {}",
                            response.error_message());
            }
        } catch (std::exception &error) {
            logger_.log("Exception: {}", error.what());
        }
    }

    awaitable<void> send_request(const protocol::NumberRequest &request) {
        request.SerializeToString(&buffer_);

        logger_.log("Sending request to {}\nRequest: {}",
                    endpoint_.address().to_string(), request);

        const auto [request_error, request_length] =
            co_await socket_.async_send_to(
                boost::asio::buffer(buffer_.data(), buffer_.size()), endpoint_);

        if (request_error) {
            throw std::runtime_error{std::format(
                "Failed to send request\nError: {}", request_error.message())};
        }

        if (request_length == 0) {
            throw std::runtime_error{
                "Failed to send request\nError: no bytes sent"};
        }
    }

    awaitable<protocol::NumberResponse> receive_response() {
        buffer_.resize(buffer_.capacity());
        const auto [response_error, response_length] =
            co_await socket_.async_receive_from(
                boost::asio::buffer(buffer_.data(), buffer_.size()), endpoint_);

        if (response_error) {
            throw std::runtime_error{
                std::format("Failed to receive response\nError: {}",
                            response_error.message())};
        }

        if (response_length == 0) {
            throw std::runtime_error{
                "Failed to receive response\nError: no bytes "
                "received"};
        }

        buffer_.resize(response_length);
        protocol::NumberResponse response;
        response.ParseFromString(buffer_);

        logger_.log("Received response from {}\nResponse: {}",
                    endpoint_.address().to_string(), response);

        co_return response;
    }

    void process_response(const protocol::NumberResponse &response) {
        if (response.error() != protocol::Error::OK) {
            return;
        }

        number_sequnces_.push_back(std::vector<NumberType>{
            response.numbers().begin(), response.numbers().end()});
        std::sort(std::execution::par, number_sequnces_.back().begin(),
                  number_sequnces_.back().end(), std::greater<NumberType>{});
    }

    void init_numbers(const protocol::NumberResponse response) {
        number_sequnces_.reserve(response.datagram_count());
    }

    void merge_numbers() {
        while (number_sequnces_.size() > 1) {
            const auto &first_sequence = *(number_sequnces_.end() - 2);
            const auto &second_sequence = *(number_sequnces_.end() - 1);
            std::vector<NumberType> merged_sequence;
            merged_sequence.reserve(first_sequence.size() +
                                    second_sequence.size());

            std::merge(first_sequence.begin(), first_sequence.end(),
                       second_sequence.begin(), second_sequence.end(),
                       std::back_inserter(merged_sequence),
                       std::greater<NumberType>{});

            number_sequnces_.erase(number_sequnces_.end() - 2,
                                   number_sequnces_.end());
            number_sequnces_.push_back(std::move(merged_sequence));
        }
    }

    void flush_numbers() {
        std::ofstream numbers_file{numbers_file_path_,
                                   std::ios::binary | std::ios::trunc};

        if (!numbers_file) {
            throw std::runtime_error{
                std::format("Failed to open numbers file. Path: {}",
                            numbers_file_path_.string())};
        }

        const auto &numbers = number_sequnces_.front();
        const auto numbers_size = numbers.size();

        numbers_file.write(reinterpret_cast<const char *>(&numbers_size),
                           sizeof(numbers_size));

        numbers_file.write(reinterpret_cast<const char *>(numbers.data()),
                           sizeof(NumberType) * numbers_size);
    }

    void print_numbers() {
        std::ifstream numbers_file{numbers_file_path_, std::ios::binary};

        size_t numbers_size{};
        numbers_file.read(reinterpret_cast<char *>(&numbers_size),
                          sizeof(numbers_size));

        std::vector<NumberType> numbers(numbers_size);

        numbers_file.read(reinterpret_cast<char *>(numbers.data()),
                          sizeof(NumberType) * numbers_size);

        for (const auto number : numbers) {
            std::cout << number << '\n';
        }
    }

    protocol::NumberRequest create_request() const {
        protocol::NumberRequest request;
        request.set_protocol_version(PROTOCOL_VERSION);
        request.set_number_count(config_.number_count());
        request.set_upper_bound(config_.upper_bound());

        return request;
    }

    protocol::Error
    validate_response(const protocol::NumberResponse &response) {
        return response.error();
    }

private:
    static constexpr uint32_t PROTOCOL_VERSION{1};
    static constexpr uint32_t DATAGRAM_SIZE{508};

    boost::asio::io_context &io_context_;
    udp_socket socket_;
    udp::endpoint endpoint_;
    std::string buffer_;
    const client::Config &config_;
    std::filesystem::path numbers_file_path_;
    utils::Logger &logger_;
    std::vector<std::vector<NumberType>> number_sequnces_;
};

int main(int argc, char *argv[]) {
    try {
        client::CommandLineOptions command_line_options;
        command_line_options.parse(argc, argv);
        client::Config config{command_line_options.config_path()};
        utils::Logger logger{command_line_options.logs_path()};

        const std::chrono::seconds sleep_time{3};
        std::this_thread::sleep_for(sleep_time);

        boost::asio::io_context io_context;
        UDPNumberSorterClient client{
            io_context, config, command_line_options.numbers_path(), logger};
        client.start();

        std::jthread io_thread([&]() { io_context.run(); });
    } catch (std::exception &error) {
        std::println(std::cerr, "Exception: {}", error.what());
    }

    return 0;
}
