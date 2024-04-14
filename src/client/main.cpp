#include "client/config.hpp"
#include "formatters.hpp"
#include "protocol.pb.h"
#include "utils/options.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/program_options.hpp>

#include <filesystem>
#include <print>

using boost::asio::ip::udp;

inline constexpr uint32_t PROTOCOL_VERSION{1};

template <typename... Args>
void log(std::format_string<Args...> format, Args &&...args) {
    std::println(std::move(format), std::forward<Args>(args)...);
}

int main(int argc, char *argv[]) {
    try {
        const auto command_line_options =
            utils::CommandLineOptions::extract(argc, argv);

        client::Config config{command_line_options.config_path()};

        boost::asio::io_context io_context;
        udp::resolver resolver{io_context};
        udp::socket socket{io_context, udp::endpoint{udp::v4(), 0}};
        udp::endpoint endpoint = *resolver
                                      .resolve(udp::v4(), config.host(),
                                               std::to_string(config.port()))
                                      .begin();

        std::string buffer;
        buffer.reserve(512);

        protocol::NumberRequest request;
        request.set_protocol_version(PROTOCOL_VERSION);
        request.set_number_count(10);
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
            sender_endpoint.address().to_string(), response);
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
