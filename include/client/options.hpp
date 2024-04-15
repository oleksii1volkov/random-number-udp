#pragma once

#include "utils/options.hpp"

namespace client {

class CommandLineOptions : public utils::CommandLineOptions {
public:
    CommandLineOptions();

    inline const std::filesystem::path &numbers_path() const {
        return numbers_path_;
    }

private:
    std::filesystem::path numbers_path_;
};

} // namespace client
