#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace utils {

namespace {

std::string get_current_time_string() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt = *std::localtime(&in_time_t);

    std::ostringstream ss;
    ss << std::put_time(&bt, "%Y-%m-%d_%H-%M-%S");
    return ss.str();
}

} // namespace

template <typename... Args>
void println(std::ostream &stream, std::format_string<Args...> format,
             Args &&...args) {
    const auto entry =
        std::format("{}", std::format(format, std::forward<Args>(args)...));

    stream << entry << "\n";
}

template <typename... Args>
void println(std::format_string<Args...> format, Args &&...args) {
    utils::println(std::cout, std::move(format), std::forward<Args>(args)...);
}

class Logger {
public:
    Logger();
    Logger(const std::filesystem::path &logs_path) {
        if (!std::filesystem::exists(logs_path)) {
            std::filesystem::create_directories(logs_path);
        }

        const auto time_string = get_current_time_string();
        std::string filename = "log_" + time_string + ".txt";
        std::filesystem::path log_path = logs_path.string() + "/" + filename;

        log_file_.open(log_path);
    }

    ~Logger() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }

    template <typename... Args>
    void log(std::format_string<Args...> format, Args &&...args) {
        const auto entry =
            std::format("[{}] {}", get_current_time_string(),
                        std::format(format, std::forward<Args>(args)...));

        utils::println("{}", entry);
        if (log_file_.is_open()) {
            log_file_ << entry << std::endl;
        }
    }

private:
    std::ofstream log_file_;
};

} // namespace utils
