#pragma once

#include <filesystem>

namespace client {

class Config {
public:
    Config();
    Config(const std::filesystem::path &path);

    inline uint16_t port() const { return port_; }
    inline const std::string &host() const { return host_; }
    inline uint64_t number_count() const { return number_count_; }
    inline double upper_bound() const { return upper_bound_; }

private:
    uint16_t port_{};
    std::string host_;
    uint64_t number_count_{};
    double upper_bound_{};
};

} // namespace client
