#include "client/options.hpp"

#include <boost/program_options.hpp>

using namespace client;

CommandLineOptions::CommandLineOptions() : utils::CommandLineOptions() {
    namespace po = boost::program_options;

    description_.add_options()(
        "numbers-path",
        po::value<std::filesystem::path>(&numbers_path_)->required(),
        "File with numbers location");
}
