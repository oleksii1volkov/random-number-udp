#include "client/config.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <format>

using namespace client;

Config::Config() : Config(std::filesystem::path{"config.json"}) {}

Config::Config(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::format(
            "Config file not found at location: {}", path.string()));
    }

    boost::property_tree::ptree root;
    boost::property_tree::read_json(path.string(), root);

    port_ = root.get<uint32_t>("port");
    host_ = root.get<std::string>("host");
    numbers_count_ = root.get<uint64_t>("numbers_count");
}
