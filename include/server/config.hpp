#pragma once

#include <filesystem>

namespace server {

class Config {
public:
    Config();
    Config(const std::filesystem::path &path);

    inline uint16_t port() const { return port_; }

private:
    uint16_t port_{};
};

} // namespace server
