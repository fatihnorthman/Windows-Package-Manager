#pragma once

#include <string>
#include <sstream>
#include <mutex>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <utility>

namespace pm {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void setLevel(LogLevel l) { level_ = l; }
    LogLevel level() const { return level_; }

    void log(LogLevel l, const std::string& msg) {
        if (l < level_) return;
        static const char* names[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
        std::lock_guard<std::mutex> lk(mtx_);
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &t);
        
        std::cerr << "[" << std::put_time(&tm, "%H:%M:%S") << "] ["
                  << names[static_cast<int>(l)] << "] " << msg << std::endl;
                  
        std::ofstream logFile("PackageManager.log", std::ios::app);
        if (logFile) {
            logFile << "[" << std::put_time(&tm, "%H:%M:%S") << "] ["
                    << names[static_cast<int>(l)] << "] " << msg << "\n";
        }
    }

    template <typename... Args>
    void write(LogLevel l, Args&&... args) {
        if (l < level_) return;
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        log(l, oss.str());
    }

    template <typename... Args>
    void debug(Args&&... args) { write(LogLevel::Debug, std::forward<Args>(args)...); }

    template <typename... Args>
    void info(Args&&... args) { write(LogLevel::Info, std::forward<Args>(args)...); }

    template <typename... Args>
    void warn(Args&&... args) { write(LogLevel::Warn, std::forward<Args>(args)...); }

    template <typename... Args>
    void error(Args&&... args) { write(LogLevel::Error, std::forward<Args>(args)...); }

private:
    Logger() = default;
    LogLevel   level_ = LogLevel::Info;
    std::mutex mtx_;
};

} // namespace pm
