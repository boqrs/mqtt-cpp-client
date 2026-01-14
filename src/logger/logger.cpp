//
// Created by wave on 2026/1/14.
//

#include "logger/logger.h"

// 移除 try-catch 块，因为我们禁用了异常
// 改为使用条件编译或返回值检查

#ifdef ENABLE_SPDLOG
// 在包含spdlog之前，定义SPDLOG_NO_EXCEPTIONS以禁用spdlog中的异常
#define SPDLOG_NO_EXCEPTIONS
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>
#include <memory>
#include <vector>

class SpdlogLogger : public EmbeddedLogger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    LoggerConfig config_;
    bool initialized_ = false;

public:
    SpdlogLogger() = default;

    void initialize(const LoggerConfig& config) override {
        if (initialized_) {
            shutdown();
        }

        config_ = config;
        std::vector<spdlog::sink_ptr> sinks;

        // 控制台输出
        if (config.consoleOutput) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(config.pattern);
            sinks.push_back(console_sink);
        }

        // 文件输出
        if (config.fileOutput && !config.logFile.empty()) {
            // 不使用异常，改用返回值检查
            auto create_file_sink = [&]() -> spdlog::sink_ptr {
                if (config.maxFileSize > 0) {
                    // 使用滚动文件
                    return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                            config.logFile, config.maxFileSize, config.maxFiles);
                } else {
                    // 使用普通文件
                    return std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                            config.logFile);
                }
            };

            auto file_sink = create_file_sink();
            if (file_sink) {
                file_sink->set_pattern(config.pattern);
                sinks.push_back(file_sink);
            } else if (config.consoleOutput) {
                // 文件创建失败，只使用控制台
                spdlog::warn("无法创建日志文件，仅使用控制台输出");
            }
        }

        // 创建日志记录器
        if (config.asyncLogging) {
            spdlog::init_thread_pool(config.asyncQueueSize, 1);
            logger_ = std::make_shared<spdlog::async_logger>(
                    "embedded", sinks.begin(), sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block);
        } else {
            logger_ = std::make_shared<spdlog::logger>(
                    "embedded", sinks.begin(), sinks.end());
        }

        // 设置日志级别
        setLevel(config.level);

        // 注册日志器
        spdlog::register_logger(logger_);
        spdlog::set_default_logger(logger_);

        initialized_ = true;

        // 记录初始化信息
        spdlog::info("SpdlogLogger 初始化完成，日志级别: {}",
                     static_cast<int>(config.level));
    }

    void shutdown() override {
        if (logger_) {
            logger_->flush();
            spdlog::drop("embedded");
            logger_.reset();
        }
        initialized_ = false;
        spdlog::shutdown();
    }

    void setLevel(LogLevel level) override {
        config_.level = level;
        if (logger_) {
            logger_->set_level(static_cast<spdlog::level::level_enum>(level));
        }
    }

    LogLevel getLevel() const override {
        return config_.level;
    }

    void flush() override {
        if (logger_) {
            logger_->flush();
        }
    }

protected:
    void log(LogLevel level, const char* fmt, ...) override {
        if (!logger_ || !initialized_) {
            return;
        }

        // 检查级别是否启用
        if (level < config_.level) {
            return;
        }

        // 格式化参数
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        // 记录日志
        logger_->log(static_cast<spdlog::level::level_enum>(level), buffer);
    }
};

#else
// 禁用spdlog时的简单实现
#include <cstdio>
#include <ctime>
#include <cstdarg>

class SimpleLogger : public EmbeddedLogger {
private:
    LoggerConfig config_;
    FILE* logFile_ = nullptr;

    const char* levelToString(LogLevel level) {
        switch(level) {
            case LogLevel::TRACE_LEVEL: return "TRACE";
            case LogLevel::DEBUG_LEVEL: return "DEBUG";
            case LogLevel::INFO_LEVEL: return "INFO";
            case LogLevel::WARN_LEVEL: return "WARN";
            case LogLevel::ERROR_LEVEL: return "ERROR";
            case LogLevel::CRITICAL_LEVEL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }

    void writeLog(LogLevel level, const char* message) {
        if (level < config_.level) return;

        // 获取当前时间
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

        // 格式化日志
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "[%s] [%s] %s\n",
                timestamp, levelToString(level), message);

        // 输出到控制台
        if (config_.consoleOutput) {
            fprintf(stdout, "%s", buffer);
            fflush(stdout);
        }

        // 输出到文件
        if (config_.fileOutput && logFile_) {
            fprintf(logFile_, "%s", buffer);
            fflush(logFile_);
        }
    }

public:
    void initialize(const LoggerConfig& config) override {
        config_ = config;

        // 打开日志文件
        if (config.fileOutput && !config.logFile.empty()) {
            logFile_ = fopen(config.logFile.c_str(), "a");
            if (!logFile_) {
                fprintf(stderr, "无法打开日志文件: %s\n", config.logFile.c_str());
            }
        }

        writeLog(LogLevel::INFO_LEVEL, "SimpleLogger 初始化完成");
    }

    void shutdown() override {
        writeLog(LogLevel::INFO_LEVEL, "SimpleLogger 关闭");
        if (logFile_) {
            fclose(logFile_);
            logFile_ = nullptr;
        }
    }

    void setLevel(LogLevel level) override {
        config_.level = level;
    }

    LogLevel getLevel() const override {
        return config_.level;
    }

    void flush() override {
        if (logFile_) {
            fflush(logFile_);
        }
        fflush(stdout);
    }

protected:
    void log(LogLevel level, const char* fmt, ...) override {
        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        writeLog(level, buffer);
    }
};
#endif

// 全局日志器实例
namespace {
    std::shared_ptr<EmbeddedLogger> global_logger = nullptr;
}

std::shared_ptr<EmbeddedLogger> getLogger() {
    if (!global_logger) {
        // 如果没有初始化，创建一个简单的控制台日志器
        static LoggerConfig defaultConfig;
        defaultConfig.consoleOutput = true;

#ifdef ENABLE_SPDLOG
        global_logger = std::make_shared<SpdlogLogger>();
#else
        global_logger = std::make_shared<SimpleLogger>();
#endif
        global_logger->initialize(defaultConfig);
    }
    return global_logger;
}

void initializeLogger(const LoggerConfig& config) {
    if (global_logger) {
        global_logger->shutdown();
    }

#ifdef ENABLE_SPDLOG
    global_logger = std::make_shared<SpdlogLogger>();
#else
    global_logger = std::make_shared<SimpleLogger>();
#endif
    global_logger->initialize(config);
}

void shutdownLogger() {
    if (global_logger) {
        global_logger->shutdown();
        global_logger.reset();
    }
}