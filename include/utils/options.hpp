#pragma once

#include <boost/program_options.hpp>

#include <filesystem>

namespace utils {

class CommandLineOptions {
public:
    CommandLineOptions();

    void parse(int argc, char *argv[]);

    inline const std::filesystem::path &config_path() const {
        return config_path_;
    }

    inline const std::filesystem::path &logs_path() const { return logs_path_; }

protected:
    boost::program_options::options_description description_;

private:
    std::filesystem::path config_path_;
    std::filesystem::path logs_path_;
};

} // namespace utils
