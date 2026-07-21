#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace kungfu::net {

// Timestamps and prints every log() call to stdout, and appends the same line to a file -
// used identically by both kungfu_server and kungfu_app so a bad run is debuggable after
// the fact on either side. Thread-safe via its own internal mutex (same reasoning as
// WsServerTransport's own clientsMutex_) - callers never need to hold anything external
// before calling log().
class Logger {
public:
    // tag is a short label prefixed on every line ("SERVER" or "CLIENT"). logFilePath's
    // parent directory is created if missing; the file is opened in append mode so
    // restarts don't lose prior runs' history.
    Logger(const std::string& tag, const std::string& logFilePath);

    void log(const std::string& message);

private:
    std::string tag_;
    std::ofstream file_;
    std::mutex mutex_;
};

}  // namespace kungfu::net
