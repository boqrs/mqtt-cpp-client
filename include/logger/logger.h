//
// Created by wave on 2026/1/14.
//
// logger.h
#pragma once

#include <string>
#include <memory>
#include <fmt/core.h>

enum class LogLevel {
    TRACE_LEVEL,
    DEBUG_LEVEL,
    INFO_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    CRITICAL_LEVEL,
    OFF_LEVEL
};

struct LoggerConfig {
    LogLevel level = LogLevel::INFO_LEVEL;
    bool consoleOutput = true;
    bool fileOutput = false;
    std::string logFile;
    bool asyncLogging = false;
    size_t asyncQueueSize = 8192;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";
};

// 抽象基类
class EmbeddedLogger {
public:
    virtual ~EmbeddedLogger() = default;

    virtual void initialize(const LoggerConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
    virtual void flush() = 0;

    virtual void trace(const std::string& msg) = 0;
    virtual void debug(const std::string& msg) = 0;
    virtual void info(const std::string& msg) = 0;
    virtual void warn(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
    virtual void critical(const std::string& msg) = 0;
};

// 全局函数
std::shared_ptr<EmbeddedLogger> getLogger();
void initializeLogger(const LoggerConfig& config);
void shutdownLogger();

// 日志宏，使用fmt库格式化字符串
#define LOG_TRACE(...)   getLogger()->trace(fmt::format(__VA_ARGS__))
#define LOG_DEBUG(...)   getLogger()->debug(fmt::format(__VA_ARGS__))
#define LOG_INFO(...)    getLogger()->info(fmt::format(__VA_ARGS__))
#define LOG_WARN(...)    getLogger()->warn(fmt::format(__VA_ARGS__))
#define LOG_ERROR(...)   getLogger()->error(fmt::format(__VA_ARGS__))
#define LOG_CRITICAL(...) getLogger()->critical(fmt::format(__VA_ARGS__))