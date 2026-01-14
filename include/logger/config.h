//
// Created by wave on 2026/1/14.
//

#pragma once

#ifdef NDEBUG
#define SPDLOG_DEFAULT_LEVEL SPDLOG_LEVEL_INFO
#else
#define SPDLOG_DEFAULT_LEVEL SPDLOG_LEVEL_DEBUG
#endif

#define LOG_TRACE_LINE(fmt, ...) \
    LOG_TRACE("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG_LINE(fmt, ...) \
    LOG_DEBUG("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO_LINE(fmt, ...) \
    LOG_INFO("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN_LINE(fmt, ...) \
    LOG_WARN("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR_LINE(fmt, ...) \
    LOG_ERROR("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_CRITICAL_LINE(fmt, ...) \
    LOG_CRITICAL("[{}:{}] " fmt, __FILE__, __LINE__, ##__VA_ARGS__)

// 模块日志宏
#define LOG_MODULE_TRACE(module, fmt, ...) \
    LOG_TRACE("[{}] " fmt, module, ##__VA_ARGS__)
#define LOG_MODULE_DEBUG(module, fmt, ...) \
    LOG_DEBUG("[{}] " fmt, module, ##__VA_ARGS__)
#define LOG_MODULE_INFO(module, fmt, ...) \
    LOG_INFO("[{}] " fmt, module, ##__VA_ARGS__)
#define LOG_MODULE_WARN(module, fmt, ...) \
    LOG_WARN("[{}] " fmt, module, ##__VA_ARGS__)
#define LOG_MODULE_ERROR(module, fmt, ...) \
    LOG_ERROR("[{}] " fmt, module, ##__VA_ARGS__)
#define LOG_MODULE_CRITICAL(module, fmt, ...) \
    LOG_CRITICAL("[{}] " fmt, module, ##__VA_ARGS__)