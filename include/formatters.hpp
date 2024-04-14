#include <format>
#include <sstream>

#include "protocol.pb.h"

// template <std::ranges::range RangeType>
// struct std::formatter<RangeType>
//     : std::formatter<std::ranges::range_value_t<RangeType>> {
//     template <typename FormatContext>
//     auto format(const RangeType &range,
//                 FormatContext &context) const -> decltype(context.out()) {
//         using ElementType = std::ranges::range_value_t<RangeType>;

//         auto out = context.out();
//         *out++ = '[';
//         for (auto element_iter = range.begin(); element_iter != range.end();
//              ++element_iter) {
//             if (element_iter != range.begin()) {
//                 out = std::copy(", ", &", "[2], out);
//             }

//             out = std::formatter<ElementType>::format(*element_iter,
//             context);
//         }
//         *out++ = ']';
//         return out;
//     }
// };

template <>
struct std::formatter<protocol::NumberRequest> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::NumberRequest &request,
                FormatContext &context) const {
        std::ostringstream request_stream;
        request_stream << "{"
                       << " protocol_version: " << request.protocol_version()
                       << ", number_count: " << request.number_count()
                       << ", number_sequence: " << request.number_sequence()
                       << " }";

        return std::formatter<std::string>::format(request_stream.str(),
                                                   context);
    }
};

template <>
struct std::formatter<protocol::NumberResponse> : std::formatter<std::string> {
    template <typename FormatContext>
    auto format(const protocol::NumberResponse &response,
                FormatContext &context) const {
        std::ostringstream response_stream;
        response_stream << "{"
                        << " protocol_version: " << response.protocol_version()
                        << ", number_count: " << response.number_count()
                        << ", number_sequence: " << response.number_sequence()
                        << ", numbers: ";

        const auto &numbers = response.numbers();
        bool first = true;
        for (const auto &number : numbers) {
            if (!first) {
                response_stream << ", ";
            }
            response_stream << number;
            first = false;
        }
        response_stream << "]" << ", error: " << response.error()
                        << ", error_message: " << response.error_message()
                        << " }";

        return std::formatter<std::string>::format(response_stream.str(),
                                                   context);
    }
};
