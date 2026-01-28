//
// Created by wave on 2026/1/23.
//

#include <chrono>
#include <iostream>

#include "logger/logger.h"
#include "service/base_service.h"

namespace swan {
namespace services {

BaseService::BaseService(const ServiceConfig& config)
    : config_(config)
    , status_(ServiceStatus::UNINITIALIZED) {

    statistics_ = Statistics{};
}

BaseService::~BaseService() {
    // 确保服务被正确关闭
    if (status_ != ServiceStatus::SHUTDOWN &&
        status_ != ServiceStatus::UNINITIALIZED) {
        LOG_ERROR("Warning: Service  {} not properly shutdown before destruction", config_.name);
        shutdown();
    }
}

std::string BaseService::getStatusString() const {
    switch (status_) {
        case ServiceStatus::UNINITIALIZED: return "UNINITIALIZED";
        case ServiceStatus::INITIALIZING:  return "INITIALIZING";
        case ServiceStatus::READY:         return "READY";
        case ServiceStatus::RUNNING:       return "RUNNING";
        case ServiceStatus::PAUSED:        return "PAUSED";
        case ServiceStatus::STOPPING:      return "STOPPING";
        case ServiceStatus::ERROR:         return "ERROR";
        case ServiceStatus::SHUTDOWN:      return "SHUTDOWN";
        default:                          return "UNKNOWN";
    }
}

bool BaseService::initialize() {
    if (status_ != ServiceStatus::UNINITIALIZED) {
        LOG_ERROR("Warning: Service  {} already initialized or in state {}", config_.name, getStatusString());
        return true;
    }

    if (!setStatus(ServiceStatus::INITIALIZING, "Initializing service")) {
        return false;
    }

    try {
        // 调用子类的初始化逻辑
        common::Result init_result = onInitialize();
        if (!init_result.isSuccess()) {
            setStatus(ServiceStatus::ERROR, init_result.getErrorMessage());

            LOG_ERROR("Error: Service  {} initialization failed: {}", config_.name, init_result.toString());
            return false;
        }

        if (!setStatus(ServiceStatus::READY, "Service initialized successfully")) {
            return false;
        }

        LOG_INFO("Info: Service  {} initialization successfully", config_.name);

        return true;

    } catch (const std::exception& e) {
        setStatus(ServiceStatus::ERROR, e.what());
        LOG_ERROR("Error: Service  {} initialization failed", config_.name, e.what());
        return false;
    }
}

bool BaseService::shutdown() {
    if (status_ == ServiceStatus::UNINITIALIZED ||
        status_ == ServiceStatus::SHUTDOWN) {
        return true;
    }

    if (!setStatus(ServiceStatus::STOPPING, "Shutting down service")) {
        return false;
    }

    try {
        // 调用子类的清理逻辑
        common::Result shutdown_result = onShutdown();
        if (!shutdown_result.isSuccess()) {
            LOG_ERROR("Warning: Service {} shutdown reported error: {}", config_.name, shutdown_result.toString());
        }

        if (!setStatus(ServiceStatus::SHUTDOWN, "Service shutdown complete")) {
            return false;
        }

        LOG_INFO("Info: Service {} shutdown successfully", config_.name, shutdown_result.toString());
        return true;

    } catch (const std::exception& e) {
        setStatus(ServiceStatus::ERROR, e.what());
        std::cerr << "Error: Service " << config_.name
                  << " shutdown failed: " << e.what() << std::endl;
        LOG_ERROR("Error: Service {} shutdown failed {}", config_.name, e.what());

        return false;
    }
}

bool BaseService::setStatus(ServiceStatus new_status, const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);

    // 状态转换验证
    switch (status_) {
        case ServiceStatus::SHUTDOWN:
            if (new_status != ServiceStatus::UNINITIALIZED) {
                LOG_ERROR("Error: Cannot change status from SHUTDOWN to {}", static_cast<int>(new_status));
                return false;
            }
            break;
        default:
            break;
    }

    if (!message.empty()) {
        LOG_DEBUG("Debug: Service {} status change {} -> {} ({}) ", config_.name, getStatusString(), getStatusString(), message);
    }
    status_ = new_status;
    return true;
}

void BaseService::updateStatistics(const common::Result& result,
                                   uint64_t execution_time_us) {
    Statistics stats = statistics_.load();

    stats.total_executions++;
    stats.total_execution_time_us += execution_time_us;
    stats.last_execution_time_us = execution_time_us;

    if (result.isSuccess()) {
        stats.successful_executions++;
    } else {
        stats.failed_executions++;
    }

    statistics_.store(stats);

    // 定期输出统计信息
    if (stats.total_executions % 100 == 0) {
        LOG_INFO("Debug: Service {}  statistics - Total: {}, Success: {}, Failed:{}, Avg time: {}",
            config_.name, stats.total_executions,
            stats.successful_executions, stats.failed_executions,
            stats.getAverageExecutionTimeMs());
    }
}

common::Result BaseService::executeWithGuard(
    const swan::protocol::ControlCommand& cmd,
    const std::shared_ptr<command::CommandContext>& context) {

    if (status_ != ServiceStatus::READY && status_ != ServiceStatus::RUNNING) {
        return common::Result::failure(
            common::command::EXECUTOR_RESOURCE_BUSY,
            "Service not ready",
            "Current status: " + getStatusString()
        );
    }

    // 验证命令
    auto validation_result = validateCommand(cmd);
    if (!validation_result.isSuccess()) {
        return validation_result;
    }

    return execute(cmd, context);
}

common::Result BaseService::execute(
    const swan::protocol::ControlCommand& cmd,
    const std::shared_ptr<command::CommandContext>& context) {

    auto start_time = std::chrono::steady_clock::now();

    try {
        // 调用子类实现的doExecute
        auto result = doExecute(cmd, context);

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        updateStatistics(result, duration.count());

        return result;

    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        common::Result error_result = common::Result::failure(
            common::command::EXECUTOR_FAILED,
            "Service execution failed",
            e.what()
        );

        updateStatistics(error_result, duration.count());

        return error_result;
    }
}

} // namespace services
} // namespace swan