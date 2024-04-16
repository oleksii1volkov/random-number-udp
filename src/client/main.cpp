#include "client/config.hpp"
#include "client/options.hpp"
#include "constants.hpp"
#include "protocol.pb.h"
#include "utils/checksum.hpp"
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
#include <thread>

using boost::asio::as_tuple_t;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using boost::asio::ip::udp;
using default_token = as_tuple_t<use_awaitable_t<>>;
using udp_socket = default_token::as_default_on_t<udp::socket>;

using namespace protocol;

class UDPNumberSorterClient {
public:
    UDPNumberSorterClient(boost::asio::io_context &io_context,
                          const client::Config &config,
                          const std::filesystem::path &numbers_file_path,
                          utils::Logger &logger)
        : io_context_{io_context},
          socket_{io_context, udp::endpoint{udp::v4(), 0}},
          buffer_(MESSAGE_MAX_SIZE, '\0'), config_{config},
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
        decltype(std::declval<NumberSequenceResponse>().numbers().Get(0))>;

    awaitable<void> run() {
        try {
            co_await send_protocol_version_request();
            auto version_response =
                co_await receive_protocol_version_response();
            if (!version_response ||
                version_response->error() != ProtocolVersionError::VERSION_OK) {
                co_return;
            }

            co_await send_number_sequence_request();

            const auto sequence_response =
                co_await receive_number_sequence_response();
            if (!sequence_response || sequence_response->error() !=
                                          NumberSequenceError::SEQUENCE_OK) {
                co_return;
            }

            init_number_sequences(*sequence_response);
            process_number_sequence_response(*sequence_response);

            uint64_t sequence_response_count{
                sequence_response->sequence_count()};

            for (uint64_t sequence_response_index{1};
                 sequence_response_index < sequence_response_count;
                 ++sequence_response_index) {
                const auto number_response =
                    co_await receive_number_sequence_response();
                if (!number_response || number_response->error() !=
                                            NumberSequenceError::SEQUENCE_OK) {
                    co_return;
                }
            }

            merge_number_sequences();
            flush_numbers();
        }

        catch (std::exception &error) {
            logger_.log("Exception: {}", error.what());
        }
    }

    awaitable<void> send_protocol_version_request() {
        const auto version_request = create_protocol_version_request();
        co_await send_request(version_request);
    }

    awaitable<std::optional<ProtocolVersionResponse>>
    receive_protocol_version_response() {
        const auto version_response =
            co_await receive_response<ProtocolVersionResponse>();

        if (version_response.error() != ProtocolVersionError::VERSION_OK) {
            logger_.log("Protocol version requirement is not met. Server "
                        "protocol version: {}. Error: {}",
                        version_response.protocol_version(),
                        version_response.error_message());
        }

        co_return version_response;
    }

    awaitable<void> send_number_sequence_request() {
        const auto number_request = create_number_sequence_request();
        co_await send_request(number_request);
    }

    awaitable<std::optional<NumberSequenceResponse>>
    receive_number_sequence_response() {
        std::optional<NumberSequenceResponse> sequence_response;

        for (uint8_t retry_index{0};
             retry_index <= SEQUENCE_RESPONSE_MAX_RETRIES_COUNT;
             ++retry_index) {
            sequence_response =
                co_await receive_response<NumberSequenceResponse>();

            if (sequence_response->error() !=
                NumberSequenceError::SEQUENCE_OK) {
                logger_.log("Number sequence response error: {}",
                            sequence_response->error_message());

                co_return std::nullopt;
            }

            const auto ack_request =
                create_number_sequence_ack_request(*sequence_response);
            co_await send_request(ack_request);

            if (ack_request.ack() == NumberSequenceAck::ACK_OK) {
                break;
            } else {
                logger_.log(
                    "Failed to acknowledge number sequence {}. Expected "
                    "checksum: {}. Actual checksum: {}. Retry: {}",
                    sequence_response->sequence_index(),
                    sequence_response->checksum(), ack_request.checksum(),
                    retry_index);
            }
        }

        co_return sequence_response;
    }

    template <typename RequestType>
    awaitable<void> send_request(const RequestType &request) {
        buffer_.clear();
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

    template <typename ResponseType>
    awaitable<ResponseType> receive_response() {
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
        ResponseType response;
        response.ParseFromString(buffer_);

        logger_.log("Received response from {}\nResponse: {}",
                    endpoint_.address().to_string(), response);

        co_return response;
    }

    void process_number_sequence_response(
        const protocol::NumberSequenceResponse &response) {
        number_sequnces_.push_back(std::vector<NumberType>{
            response.numbers().begin(), response.numbers().end()});
        std::sort(std::execution::par, number_sequnces_.back().begin(),
                  number_sequnces_.back().end(), std::greater<NumberType>{});
    }

    void
    init_number_sequences(const protocol::NumberSequenceResponse response) {
        number_sequnces_.reserve(response.sequence_count());
    }

    void merge_number_sequences() {
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

    void flush_numbers() const {
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

    void print_numbers() const {
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

    ProtocolVersionRequest create_protocol_version_request() const {
        ProtocolVersionRequest request;
        request.set_protocol_version(PROTOCOL_VERSION);

        return request;
    }

    NumberSequenceRequest create_number_sequence_request() const {
        NumberSequenceRequest request;
        request.set_number_count(config_.number_count());
        request.set_upper_bound(config_.upper_bound());

        return request;
    }

    NumberSequenceAckRequest create_number_sequence_ack_request(
        const NumberSequenceResponse &response) const {
        NumberSequenceAckRequest ack_request;
        ack_request.set_sequence_index(response.sequence_index());
        ack_request.set_checksum(utils::calculate_checksum(response.numbers()));
        ack_request.set_ack((response.checksum() == ack_request.checksum())
                                ? protocol::NumberSequenceAck::ACK_OK
                                : protocol::NumberSequenceAck::ACK_INVALID);

        return ack_request;
    }

private:
    static constexpr uint32_t PROTOCOL_VERSION{1};

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
        utils::println(std::cerr, "Exception: {}", error.what());
    }

    return 0;
}
