//
// Created by wave on 2026/1/23.
//
// dispatcher.cpp 修复所有锁

#include <iostream>

#include "protocol/dispatch.h"
#include "logger/logger.h"

namespace swan {
namespace protocol {

CommandDispatcher::CommandDispatcher()
    : max_queue_size_(1000)
    , enable_queue_(true)
    , service_factory_(services::ServiceFactory::instance()) {

    queue_running_ = true;
    queue_processor_ = std::thread(&CommandDispatcher::queueProcessorThread, this);
    LOG_INFO("CommandDispatcher initialized with queue system");
}

CommandDispatcher::~CommandDispatcher() {
    // 停止队列处理线程
    queue_running_ = false;
    queue_cv_.notify_all();

    if (queue_processor_.joinable()) {
        queue_processor_.join();
    }

    LOG_INFO("CommandDispatcher shutdown");
}

common::Result CommandDispatcher::dispatch(
    const device::UnifiedMessage& message,
    std::shared_ptr<command::CommandContext> context) {

    if (!message.has_payload()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing payload in message"
        );
    }

    const auto& payload = message.payload();
    std::string action_type = payload.action_type();

    if (action_type.empty()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing action_type in payload"
        );
    }

    // 从消息中提取 ControlCommand
    if (!payload.has_device_cmd()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing device_cmd in payload"
        );
    }

    return dispatch(payload.device_cmd(), action_type, context);
}

common::Result CommandDispatcher::dispatch(
    const device::ControlCommand& cmd,
    const std::string& action_type,
    std::shared_ptr<command::CommandContext> context) {

    // 查找对应的服务
    auto service = service_factory_.discoverServiceByAction(action_type);
    if (!service) {
        LOG_ERROR("Error: No service found for action type: {}", action_type);
        return common::Result::failure(
            common::protocol::DISPATCHER_NO_HANDLER,
            "No service handler for action",
            action_type
        );
    }

    // 验证服务状态
    auto status = service->getStatus();
    if (status != "READY" && status != "RUNNING") {
        LOG_ERROR("Service: {}  not ready, status: {}", service->getName(), status);
        return common::Result::failure(
            common::command::EXECUTOR_RESOURCE_BUSY,
            "Service not ready",
            "Status: " + status
        );
    }

    // 执行命令
    try {
        LOG_INFO("Dispatching command: {} to service {} ", cmd.cmd(), service->getName());
        auto result = service->execute(cmd, context);
        // 将 action_type 设置为结果数据
        result.setData(action_type);
        return result;

    } catch (const std::exception& e) {
        LOG_ERROR(" Exception during command dispatch: {}", e.what());
        return common::Result::failure(
            common::command::EXECUTOR_FAILED,
            "Command execution failed",
            e.what()
        );
    }
}

bool CommandDispatcher::registerService(ServicePtr service) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    std::string action_type = service->getSupportedActionType();
    if (services_.find(action_type) != services_.end()) {
        LOG_ERROR("Service already registered for action type: {}", action_type);
        return false;
    }

    services_[action_type] = service;
    LOG_INFO("Registered service for action: {}", action_type);
    return true;
}

CommandDispatcher::ServicePtr CommandDispatcher::findServiceForAction(const std::string& action_type) {
    std::lock_guard<std::mutex> lock(services_mutex_);

    auto it = services_.find(action_type);
    if (it != services_.end()) {
        return it->second;
    }

    // 如果没有直接匹配，尝试遍历所有服务
    for (const auto& [key, service] : services_) {
        if (service->getSupportedActionType() == action_type) {
            return service;
        }
    }

    return nullptr;
}

// 修复所有锁的使用，将 std::lock<std::mutex> 改为 std::lock_guard<std::mutex>
CommandDispatcher::ServicePtr CommandDispatcher::getService(const std::string& action_type) const {
    std::lock_guard<std::mutex> lock(services_mutex_);

    auto it = services_.find(action_type);
    return (it != services_.end()) ? it->second : nullptr;
}

bool CommandDispatcher::enqueueCommand(QueuedCommand&& command) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (command_queue_.size() >= max_queue_size_) {
        return false;
    }

    command_queue_.push(std::move(command));
    queue_cv_.notify_one();
    return true;
}

std::optional<CommandDispatcher::QueuedCommand> CommandDispatcher::dequeueCommand() {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (command_queue_.empty()) {
        return std::nullopt;
    }

    auto cmd = std::move(command_queue_.front());
    command_queue_.pop();
    return cmd;
}

size_t CommandDispatcher::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return command_queue_.size();
}

} // namespace protocol
} // namespace swan