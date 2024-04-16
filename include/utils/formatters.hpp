#include <format>
#include <ranges>
#include <sstream>

#include "protocol.pb.h"

template <>
struct std::formatter<protocol::ProtocolVersionRequest>
    : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::ProtocolVersionRequest &request,
                FormatContext &context) const {
        std::ostringstream request_stream;
        request_stream << "{ "
                       << "protocol_version: " << request.protocol_version()
                       << " }";

        return std::formatter<std::string>::format(request_stream.str(),
                                                   context);
    }
};

template <>
struct std::formatter<protocol::ProtocolVersionResponse>
    : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::ProtocolVersionResponse &response,
                FormatContext &context) const {
        std::ostringstream response_stream;
        response_stream << "{ "
                        << "protocol_version: " << response.protocol_version()
                        << ", error: " << response.error()
                        << ", error_message: \"" << response.error_message()
                        << "\" }";

        return std::formatter<std::string>::format(response_stream.str(),
                                                   context);
    }
};

template <>
struct std::formatter<protocol::NumberSequenceRequest>
    : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::NumberSequenceRequest &request,
                FormatContext &context) const {
        std::ostringstream request_stream;
        request_stream << "{ " << "number_count: " << request.number_count()
                       << ", upper_bound: " << request.upper_bound() << " }";

        return std::formatter<std::string>::format(request_stream.str(),
                                                   context);
    }
};

template <>
struct std::formatter<protocol::NumberSequenceResponse>
    : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::NumberSequenceResponse &response,
                FormatContext &context) const {
        std::ostringstream response_stream;
        response_stream << "{ " << "number_count: " << response.number_count()
                        << ", sequence_index: " << response.sequence_index()
                        << ", sequence_count: " << response.sequence_count()
                        << ", sequence_number_count: "
                        << response.sequence_number_count() << ", numbers: [";

        // response_stream << "...";

        const auto &numbers = response.numbers();
        if (!numbers.empty()) {
            response_stream << numbers[0];
            for (const auto &number : numbers | std::views::drop(1)) {
                {
                    response_stream << ", " << number;
                }
            }
        }

        response_stream << "]" << ", checksum: " << response.checksum()
                        << ", error: " << response.error()
                        << ", error_message: \"" << response.error_message()
                        << "\" }";

        return std::formatter<std::string>::format(response_stream.str(),
                                                   context);
    }
};

template <>
struct std::formatter<protocol::NumberSequenceAckRequest>
    : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::NumberSequenceAckRequest &request,
                FormatContext &context) const {
        std::ostringstream request_stream;
        request_stream << "{" << " sequence_index: " << request.sequence_index()
                       << ", ack: " << request.ack()
                       << ", checksum: " << request.checksum() << " }";

        return std::formatter<std::string>::format(request_stream.str(),
                                                   context);
    }
};
