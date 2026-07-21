#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace kungfu::net {

namespace {

// Millisecond precision matches this project's existing timing model (MoveStarted's own
// elapsedMs, etc.) so log lines can be correlated with in-game timing when debugging.
std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmBuf{};
    localtime_s(&tmBuf, &nowTimeT);  // thread-safe variant - this project is Windows-only already

    std::ostringstream out;
    out << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return out.str();
}

}  // namespace

Logger::Logger(const std::string& tag, const std::string& logFilePath) : tag_(tag) {
    const std::filesystem::path path(logFilePath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    file_.open(logFilePath, std::ios::app);
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string line = "[" + timestampNow() + "] [" + tag_ + "] " + message;
    std::cout << line << std::endl;
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
}

}  // namespace kungfu::net
