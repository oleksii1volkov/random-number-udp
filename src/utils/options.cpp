#include "utils/options.hpp"

#include <boost/program_options.hpp>

using namespace utils;

CommandLineOptions::CommandLineOptions() : description_{"Options"} {
    namespace po = boost::program_options;

    description_.add_options()(
        "config-path",
        po::value<std::filesystem::path>(&config_path_)->required(),
        "Config file location");
    description_.add_options()(
        "logs-path", po::value<std::filesystem::path>(&logs_path_)->required(),
        "Location of the logs directory");
}

void CommandLineOptions::parse(int argc, char *argv[]) {
    namespace po = boost::program_options;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, description_), vm);
    po::notify(vm);
}
