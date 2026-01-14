//
// Created by wave on 2026/1/14.
//
#pragma once

#include <memory>
#include <string>

// 避免 DEBUG 宏冲突，使用不同的命名
enum class LogLevel {
    TRACE_LEVEL = 0,
    DEBUG_LEVEL = 1,    // 改为 DEBUG_LEVEL 避免与 DEBUG 宏冲突
    INFO_LEVEL = 2,
    WARN_LEVEL = 3,
    ERROR_LEVEL = 4,
    CRITICAL_LEVEL = 5,
    OFF_LEVEL = 6
};

// 日志配置
struct LoggerConfig {
    LogLevel level = LogLevel::INFO_LEVEL;
    std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v";
    std::string logFile = "";
    size_t maxFileSize = 5 * 1024 * 1024;  // 5MB
    size_t maxFiles = 3;
    bool consoleOutput = true;
    bool fileOutput = false;
    bool asyncLogging = false;
    size_t asyncQueueSize = 8192;  // 异步队列大小
};

// 嵌入式日志器接口
class EmbeddedLogger {
public:
    virtual ~EmbeddedLogger() = default;

    // 模板方法用于格式化输出
    template<typename... Args>
    void trace(const char* fmt, Args... args) {
        log(LogLevel::TRACE_LEVEL, fmt, args...);
    }

    template<typename... Args>
    void debug(const char* fmt, Args... args) {
        log(LogLevel::DEBUG_LEVEL, fmt, args...);  // 使用 DEBUG_LEVEL
    }

    template<typename... Args>
    void info(const char* fmt, Args... args) {
        log(LogLevel::INFO_LEVEL, fmt, args...);
    }

    template<typename... Args>
    void warn(const char* fmt, Args... args) {
        log(LogLevel::WARN_LEVEL, fmt, args...);
    }

    template<typename... Args>
    void error(const char* fmt, Args... args) {
        log(LogLevel::ERROR_LEVEL, fmt, args...);
    }

    template<typename... Args>
    void critical(const char* fmt, Args... args) {
        log(LogLevel::CRITICAL_LEVEL, fmt, args...);
    }

    // 纯虚函数，由具体实现类重写
    virtual void initialize(const LoggerConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual void setLevel(LogLevel level) = 0;
    virtual LogLevel getLevel() const = 0;
    virtual void flush() = 0;

protected:
    // 实际执行日志记录的方法
    virtual void log(LogLevel level, const char* fmt, ...) = 0;
};

// 获取全局日志器实例
std::shared_ptr<EmbeddedLogger> getLogger();
void initializeLogger(const LoggerConfig& config = LoggerConfig());
void shutdownLogger();

// 便利宏
#define LOG_TRACE(...)   getLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)   getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)    getLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)    getLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)   getLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) getLogger()->critical(__VA_ARGS__)
#define LOG_FLUSH()      getLogger()->flush()