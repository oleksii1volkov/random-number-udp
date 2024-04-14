#include "utils/options.hpp"

#include <boost/program_options.hpp>

using namespace utils;

CommandLineOptions CommandLineOptions::extract(int argc, char *argv[]) {
    namespace po = boost::program_options;

    std::filesystem::path config_path;
    std::filesystem::path logs_path;

    po::options_description description("Options");
    description.add_options()("config-path",
                              po::value<std::filesystem::path>(&config_path),
                              "Config file location");
    description.add_options()("logs-path",
                              po::value<std::filesystem::path>(&logs_path),
                              "Location of the logs directory");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::notify(vm);

    return {config_path, logs_path};
}

CommandLineOptions::CommandLineOptions(const std::filesystem::path &config_path,
                                       const std::filesystem::path &logs_path)
    : config_path_{config_path}, logs_path_{logs_path} {}
