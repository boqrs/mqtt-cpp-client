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
#include <atomic>
#include <chrono>  // 新增：用于等待刷盘
#include <thread>   // 新增：用于sleep

// ======================= 实现类定义 =======================
class EmbeddedLoggerSpdlog final : public EmbeddedLogger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    LogLevel current_level_ = LogLevel::INFO_LEVEL;
    bool initialized_ = false;
    std::recursive_mutex mutex_;
    bool console_output_ = true;

    // 新增：全局线程池管理（静态+原子变量，确保线程安全）
    static std::atomic<bool> g_thread_pool_initialized;  // 线程池是否初始化
    static std::atomic<bool> g_thread_pool_cleaned;      // 线程池是否已清理

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
        console_sink->set_color(spdlog::level::trace, "\033[90m");    // 灰色 (亮黑色)
        console_sink->set_color(spdlog::level::debug, "\033[36m");    // 青色
        console_sink->set_color(spdlog::level::info, "\033[32m");     // 绿色
        console_sink->set_color(spdlog::level::warn, "\033[33m");     // 黄色
        console_sink->set_color(spdlog::level::err, "\033[31m");      // 红色
        console_sink->set_color(spdlog::level::critical, "\033[1;31m"); // 红色加粗
        console_sink->set_color(spdlog::level::info, "\033[32m");  // 绿色
    }

    // 适配旧版spdlog：安全清理全局线程池（移除stop()调用）
    void clean_global_thread_pool() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (g_thread_pool_initialized.load() && !g_thread_pool_cleaned.load()) {
            try {
                // 旧版spdlog无stop()，改用：1. 强制刷盘 2. 延迟等待 3. 全局shutdown
                if (logger_) {
                    logger_->flush(); // 强制刷盘所有未写入的日志
                    // 等待200ms，给异步线程池足够时间完成刷盘（旧版关键！）
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                // 标记线程池已清理，避免重复操作
                g_thread_pool_cleaned.store(true);
                logger_->info("异步日志线程池已安全清理（适配旧版spdlog）");
            } catch (const std::exception& e) {
                std::cerr << "清理日志线程池失败: " << e.what() << std::endl;
            }
        }
    }

public:
    EmbeddedLoggerSpdlog() = default;

    // 修改析构函数：适配旧版spdlog，先清理线程池，再销毁logger
    ~EmbeddedLoggerSpdlog() override {
        // 先清理线程池（旧版无stop，改用刷盘+等待）
        clean_global_thread_pool();
        do_shutdown();
    }

    void initialize(const LoggerConfig& config) override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        if (initialized_) {
            do_shutdown();
            // 如果之前清理过线程池，重置状态
            if (g_thread_pool_cleaned.load()) {
                g_thread_pool_initialized.store(false);
                g_thread_pool_cleaned.store(false);
            }
        }

        console_output_ = config.consoleOutput;

        try {
            std::vector<spdlog::sink_ptr> sinks;

            // 控制台输出
            if (config.consoleOutput) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_pattern(config.pattern);
                setup_console_colors(console_sink);
                sinks.push_back(console_sink);
            }

            // 文件输出
            if (config.fileOutput && !config.logFile.empty()) {
                try {
                    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                            config.logFile, true);
                    file_sink->set_pattern(config.pattern);
                    sinks.push_back(file_sink);
                } catch (const spdlog::spdlog_ex& e) {
                    std::cerr << "创建文件日志失败: " << e.what() << std::endl;
                }
            }

            if (sinks.empty()) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
                setup_console_colors(console_sink);
                sinks.push_back(console_sink);
            }

            // 创建日志器 - 适配旧版spdlog的线程池初始化
            if (config.asyncLogging && config.asyncQueueSize > 0) {
                // 原子变量确保线程池仅初始化一次
                if (!g_thread_pool_initialized.load()) {
                    spdlog::init_thread_pool(config.asyncQueueSize, 1);
                    g_thread_pool_initialized.store(true);
                    g_thread_pool_cleaned.store(false);
                }

                logger_ = std::make_shared<spdlog::async_logger>(
                        "embedded_logger",
                        sinks.begin(),
                        sinks.end(),
                        spdlog::thread_pool(),
                        spdlog::async_overflow_policy::block); // 队列满时阻塞，避免丢日志
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

            if (config.asyncLogging && config.asyncQueueSize > 0) {
                logger_->info("Asynchronous logging thread pool initialized successfully, queue size: {}", config.asyncQueueSize);
            }

            logger_->info("Log system initialized successfully, level: {}, asynchronous: {}",
                          static_cast<int>(config.level),
                          config.asyncLogging ? "yes" : "no");

        } catch (const std::exception& e) {
            std::cerr << "Log system initialization failed: " << e.what()
                      << "，Will use basic console logging" << std::endl;

            // 创建简单的控制台日志器作为后备
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            setup_console_colors(console_sink);

            logger_ = std::make_shared<spdlog::logger>("fallback_logger", console_sink);
            logger_->set_level(spdlog::level::info);
            current_level_ = LogLevel::INFO_LEVEL;
            console_output_ = true;
            initialized_ = true;

            logger_->error("Log system initialization failed, falling back to backup mode: {}", e.what());
        }
    }

    void shutdown() override {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        // 先清理线程池（适配旧版spdlog）
        clean_global_thread_pool();
        do_shutdown();
    }

    void do_shutdown() {
        if (logger_) {
            logger_->flush();          // 强制刷盘所有未写入的日志
            // 延长等待时间到300ms，确保旧版spdlog异步刷盘完成
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
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

    // 日志方法保持不变
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

// 初始化静态原子变量
std::atomic<bool> EmbeddedLoggerSpdlog::g_thread_pool_initialized(false);
std::atomic<bool> EmbeddedLoggerSpdlog::g_thread_pool_cleaned(false);

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
        default_config.asyncLogging = false;  // 默认关闭异步，避免新手踩坑
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
    // 如果已有日志器，先关闭（含线程池清理）
    if (global_logger) {
        global_logger->shutdown();
        global_logger.reset();
    }

    auto logger = std::make_shared<EmbeddedLoggerSpdlog>();
    logger->initialize(config);
    global_logger = logger;
}

void shutdownLogger() {
    std::lock_guard<std::mutex> lock(global_logger_mutex);
    if (global_logger) {
        global_logger->shutdown();
        spdlog::shutdown();
        global_logger.reset();
    }
}