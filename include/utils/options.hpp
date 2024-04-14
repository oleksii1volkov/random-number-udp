#pragma once

#include <filesystem>

namespace utils {

class CommandLineOptions {
public:
    static CommandLineOptions extract(int argc, char *argv[]);

    CommandLineOptions(const std::filesystem::path &config_path,
                       const std::filesystem::path &logs_path);

    inline const std::filesystem::path &config_path() const {
        return config_path_;
    }

    inline const std::filesystem::path &logs_path() const { return logs_path_; }

private:
    std::filesystem::path config_path_;
    std::filesystem::path logs_path_;
};

} // namespace utils
