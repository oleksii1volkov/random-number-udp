#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>

#include <print>

#include "protocol.pb.h"

using boost::asio::ip::udp;

enum { max_length = 1024 };

inline constexpr uint32_t PROTOCOL_VERSION{1};

template <typename... Args>
void log(std::format_string<Args...> format, Args &&...args) {
    std::println(std::move(format), std::forward<Args>(args)...);
}

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: blocking_udp_echo_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        udp::resolver resolver{io_context};
        udp::socket socket{io_context, udp::endpoint{udp::v4(), 0}};
        udp::endpoint endpoint =
            *resolver.resolve(udp::v4(), argv[1], argv[2]).begin();

        std::string buffer;
        buffer.reserve(max_length);

        protocol::NumberRequest request;
        request.set_protocol_version(PROTOCOL_VERSION);
        request.SerializeToString(&buffer);

        socket.send_to(boost::asio::buffer(buffer.data(), buffer.size()),
                       endpoint);
        buffer.resize(buffer.capacity());
        log("Sent request to {}.", endpoint.address().to_string());

        udp::endpoint sender_endpoint;
        size_t response_length = socket.receive_from(
            boost::asio::buffer(buffer.data(), buffer.size()), sender_endpoint);
        buffer.resize(response_length);

        protocol::NumberResponse response;
        response.ParseFromString(buffer);

        log("Received response from {}. Response: {}",
            sender_endpoint.address().to_string(),
            static_cast<int>(response.error()));
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
