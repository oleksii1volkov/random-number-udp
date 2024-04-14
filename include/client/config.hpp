#pragma once

#include <filesystem>

namespace client {

class Config {
public:
    Config();
    Config(const std::filesystem::path &path);

    inline uint16_t port() const { return port_; }
    inline const std::string &host() const { return host_; }
    inline uint64_t numbers_count() const { return numbers_count_; }

private:
    uint16_t port_{};
    std::string host_;
    uint64_t numbers_count_{};
};

} // namespace client
