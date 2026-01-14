//
// Created by wave on 2026/1/14.
//
// logger.cpp
#include "logger/logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <cstdarg>

// ======================= 实现类定义 =======================
class EmbeddedLoggerSpdlog final : public EmbeddedLogger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    LogLevel current_level_ = LogLevel::INFO_LEVEL;
    bool initialized_ = false;
    std::recursive_mutex mutex_;
    bool console_output_ = true;

    // 将我们的日志级别转换为 spdlog 的级别
    static spdlog::level::level_enum to_spdlog_level(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE_LEVEL: return spdlog::level::trace;
            case LogLevel::DEBUG_LEVEL: return spdlog::level::debug;
            case LogLevel::INFO_LEVEL: return spdlog::level::info;
            case LogLevel::WARN_LEVEL: return spdlog::level::warn;
            case LogLevel::ERROR_LEVEL: return spdlog::level::err;
            case LogLevel::CRITICAL_LEVEL: return spdlog::level::critical;
            case LogLevel::OFF_LEVEL: return spdlog::level::off;
            default: return spdlog::level::info;
        }
    }

    // 设置控制台 sink 的颜色
    void setup_console_colors(const std::shared_ptr<spdlog::sinks::stdout_color_sink_mt>& console_sink) {
        if (!console_sink) return;

        // 为不同日志级别设置颜色（ANSI 颜色代码）
        // 格式: \033[颜色代码m
        console_sink->set_color(spdlog::level::trace, "\033[90m");    // 灰色 (亮黑色)
        console_sink->set_color(spdlog::level::debug, "\033[36m");    // 青色
        console_sink->set_color(spdlog::level::info, "\033[32m");     // 绿色
        console_sink->set_color(spdlog::level::warn, "\033[33m");     // 黄色
        console_sink->set_color(spdlog::level::err, "\033[31m");      // 红色
        console_sink->set_color(spdlog::level::critical, "\033[1;31m"); // 红色加粗

        // 设置默认颜色
        console_sink->set_color(spdlog::level::info, "\033[32m");  // 绿色

        // 设置 level 字符串的颜色
        // 注意：spdlog 的 set_color 方法可能会覆盖整个消息的颜色
        // 我们这里只设置 level 部分的颜色
    }

public:
    EmbeddedLoggerSpdlog() = default;

    ~EmbeddedLoggerSpdlog() override {
        do_shutdown();
    }

    void initialize(const LoggerConfig& config) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (initialized_) {
            do_shutdown();
        }

        console_output_ = config.consoleOutput;

        try {
            std::vector<spdlog::sink_ptr> sinks;

            // 控制台输出
            if (config.consoleOutput) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_pattern(config.pattern);

                // 设置颜色
                setup_console_colors(console_sink);

                sinks.push_back(console_sink);
            }

            // 文件输出 - 文件通常不需要颜色
            if (config.fileOutput && !config.logFile.empty()) {
                try {
                    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                            config.logFile, true);
                    // 文件日志通常不需要颜色转义码
                    file_sink->set_pattern(config.pattern);
                    sinks.push_back(file_sink);
                } catch (const spdlog::spdlog_ex& e) {
                    std::cerr << "创建文件日志失败: " << e.what() << std::endl;
                }
            }

            if (sinks.empty()) {
                // 创建默认控制台输出
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

                // 设置颜色
                setup_console_colors(console_sink);

                sinks.push_back(console_sink);
            }

            // 创建日志器
            if (config.asyncLogging && config.asyncQueueSize > 0) {
                // 确保线程池只初始化一次
                static bool thread_pool_initialized = false;
                if (!thread_pool_initialized) {
                    spdlog::init_thread_pool(config.asyncQueueSize, 1);
                    thread_pool_initialized = true;
                }

                logger_ = std::make_shared<spdlog::async_logger>(
                        "embedded_logger",
                        sinks.begin(),
                        sinks.end(),
                        spdlog::thread_pool(),
                        spdlog::async_overflow_policy::block);
            } else {
                logger_ = std::make_shared<spdlog::logger>(
                        "embedded_logger",
                        sinks.begin(),
                        sinks.end());
            }

            // 设置级别
            logger_->set_level(to_spdlog_level(config.level));
            current_level_ = config.level;

            // 注册并设置为默认
            spdlog::register_logger(logger_);
            spdlog::set_default_logger(logger_);

            initialized_ = true;

            // 记录初始化成功
            logger_->info("日志系统初始化完成，级别: {}, 异步: {}",
                          static_cast<int>(config.level),
                          config.asyncLogging ? "是" : "否");

        } catch (const std::exception& e) {
            std::cerr << "日志系统初始化失败: " << e.what()
                      << "，将使用基础控制台日志" << std::endl;

            // 创建简单的控制台日志器作为后备
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

            // 设置颜色
            setup_console_colors(console_sink);

            logger_ = std::make_shared<spdlog::logger>("fallback_logger", console_sink);
            logger_->set_level(spdlog::level::info);
            current_level_ = LogLevel::INFO_LEVEL;
            console_output_ = true;
            initialized_ = true;

            logger_->error("日志系统初始化失败，使用后备模式: {}", e.what());
        }
    }

    void shutdown() override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        do_shutdown();
    }

    // 内部的不加锁关闭方法
    void do_shutdown() {
        if (logger_) {
            logger_->flush();
            try {
                spdlog::drop("embedded_logger");
            } catch (...) {
                // 忽略异常
            }
            logger_.reset();
        }

        if (initialized_) {
            initialized_ = false;
        }
    }

    void setLevel(LogLevel level) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        current_level_ = level;
        if (logger_) {
            logger_->set_level(to_spdlog_level(level));
        }
    }

    LogLevel getLevel() const override {
        return current_level_;
    }

    void flush() override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (logger_) {
            logger_->flush();
        }
    }

    // 实现日志方法
    void trace(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::TRACE_LEVEL < current_level_) return;
        logger_->trace("{}", msg);
    }

    void debug(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::DEBUG_LEVEL < current_level_) return;
        logger_->debug("{}", msg);
    }

    void info(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::INFO_LEVEL < current_level_) return;
        logger_->info("{}", msg);
    }

    void warn(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::WARN_LEVEL < current_level_) return;
        logger_->warn("{}", msg);
    }

    void error(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::ERROR_LEVEL < current_level_) return;
        logger_->error("{}", msg);
    }

    void critical(const std::string& msg) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!logger_ || LogLevel::CRITICAL_LEVEL < current_level_) return;
        logger_->critical("{}", msg);
    }
};

// ======================= 全局实例管理 =======================
namespace {
    std::shared_ptr<EmbeddedLogger> global_logger = nullptr;
    std::mutex global_logger_mutex;

    // 创建默认日志器
    std::shared_ptr<EmbeddedLogger> create_default_logger() {
        auto logger = std::make_shared<EmbeddedLoggerSpdlog>();

        LoggerConfig default_config;
        default_config.level = LogLevel::INFO_LEVEL;
        default_config.consoleOutput = true;
        default_config.fileOutput = false;
        default_config.asyncLogging = false;
        default_config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v";

        logger->initialize(default_config);
        return logger;
    }
}

// ======================= 公开接口实现 =======================
std::shared_ptr<EmbeddedLogger> getLogger() {
    std::lock_guard<std::mutex> lock(global_logger_mutex);

    if (!global_logger) {
        global_logger = create_default_logger();
    }

    return global_logger;
}

void initializeLogger(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(global_logger_mutex);

    // 如果已有日志器，先关闭
    if (global_logger) {
        global_logger->shutdown();
        global_logger.reset();
    }

    // 创建并初始化新的日志器
    auto logger = std::make_shared<EmbeddedLoggerSpdlog>();
    logger->initialize(config);

    global_logger = logger;
}

void shutdownLogger() {
    std::lock_guard<std::mutex> lock(global_logger_mutex);

    if (global_logger) {
        global_logger->shutdown();
        global_logger.reset();
    }
}