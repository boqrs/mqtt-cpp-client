//
// Created by wave on 2026/1/23.
//
// dispatcher.cpp 修复所有锁

#include "logger/logger.h"
#include "protocol/dispatch.h"

namespace swan {
namespace protocol {

    static std::string threadIdToString(std::thread::id tid) {
        std::ostringstream oss;
        oss << tid;
        return oss.str();
    }


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
    common::Result CommandDispatcher::dispatchUnifiedMessage(
        const device::UnifiedMessage& message,
        const std::shared_ptr<swan::protocol::command::CommandContext> context) {

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

        return dispatchControlCommand(payload.device_cmd(), action_type, context);
    }

common::Result CommandDispatcher::dispatchControlCommand(
    const device::ControlCommand& cmd,
    const std::string& action_type,
    const std::shared_ptr<swan::protocol::command::CommandContext>& context) {

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

    // 第一步：判断执行策略（直接/入队/拒绝）
    ExecutionStrategy strategy = determineStrategy(service, context);
    LOG_DEBUG("Determined execution strategy for [action: {}, cmd: {}]: {}",
              action_type, cmd.cmd(), static_cast<int>(strategy));

    // 第二步：根据策略处理
    switch (strategy) {
        case ExecutionStrategy::DIRECT: {
            // 直接执行（原有逻辑）
            return executeImmediately(service, cmd, context);
        }

        case ExecutionStrategy::QUEUED: {
            // 入队异步处理
            QueuedCommand queued_cmd{
                .cmd = cmd,
                .context = context,
                .action_type = action_type,
                .enqueue_time = std::chrono::steady_clock::now()
            };

            if (!enqueueCommand(std::move(queued_cmd))) {
                // 队列已满，返回拒绝
                LOG_ERROR("Command queue is full (max size: {}), reject command [action: {}]",
                          max_queue_size_, action_type);
                return common::Result::failure(
                    common::protocol::DISPATCHER_QUEUE_FULL,
                    "Command queue is full",
                    "Max size: " + std::to_string(max_queue_size_)
                );
            }

            LOG_INFO("Command queued successfully [action: {}, request_id: {}], queue size: {}",
                     action_type, context->getRequestId(), getQueueSize());
            return common::Result::success("Command queued for asynchronous execution")
                .setData(std::string("queued:" + action_type));
        }

        case ExecutionStrategy::REJECTED: {
            // 策略拒绝（队列满+服务忙）
            LOG_ERROR("Command rejected [action: {}] - service busy and queue full",
                      action_type);
            return common::Result::failure(
                common::protocol::DISPATCHER_REJECTED,
                "Command rejected - service busy and queue full",
                action_type
            );
        }

        default: {
            LOG_ERROR("Unknown execution strategy for command [action: {}]", action_type);
            return common::Result::failure(
                common::protocol::DISPATCHER_INVALID_STRATEGY,
                "Unknown execution strategy",
                std::to_string(static_cast<int>(strategy))
            );
        }
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

void CommandDispatcher::queueProcessorThread() {
    // 修复：thread::id转字符串后输出
    LOG_INFO("Command queue processor thread started (tid: {})",
             threadIdToString(std::this_thread::get_id()));

    // 循环处理队列，直到收到停止信号
    while (queue_running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // 等待条件：队列非空 或 线程需要退出
        queue_cv_.wait(lock, [this]() {
            return !queue_running_ || !command_queue_.empty();
        });

        // 线程退出信号优先处理
        if (!queue_running_) {
            LOG_INFO("Queue processor thread received shutdown signal, exiting");
            break;
        }

        // 取出队列中的命令（此时队列非空）
        QueuedCommand cmd = std::move(command_queue_.front());
        command_queue_.pop();
        lock.unlock(); // 释放队列锁，避免执行命令时阻塞入队

        // 执行队列中的命令（独立处理，捕获所有异常）
        try {
            LOG_DEBUG("Processing queued command [action: {}, request_id: {}], "
                      "enqueue time: {}ms ago",
                      cmd.action_type,
                      cmd.context->getRequestId(),
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - cmd.enqueue_time
                      ).count());

            // 查找服务并执行命令
            auto service = findServiceForAction(cmd.action_type);
            if (!service) {
                LOG_ERROR("No service found for queued command action: {}", cmd.action_type);
                // 向上下文返回错误响应
                cmd.context->sendResponse(common::Result::failure(
                    common::protocol::DISPATCHER_NO_HANDLER,
                    "No service handler for queued command",
                    cmd.action_type
                ));
                continue;
            }

            // 执行命令并发送响应
            common::Result result = executeImmediately(service, cmd.cmd, cmd.context);
            cmd.context->sendResponse(result);

            LOG_DEBUG("Queued command processed successfully [action: {}, request_id: {}]",
                      cmd.action_type, cmd.context->getRequestId());

        } catch (const std::exception& e) {
            LOG_ERROR("Exception processing queued command [action: {}]: {}",
                      cmd.action_type, e.what());
            // 异常时发送错误响应
            cmd.context->sendResponse(common::Result::failure(
                common::command::EXECUTOR_FAILED,
                "Exception processing queued command",
                e.what()
            ));
        } catch (...) {
            LOG_ERROR("Unknown exception processing queued command [action: {}]",
                      cmd.action_type);
            cmd.context->sendResponse(common::Result::failure(
                common::command::EXECUTOR_FAILED,
                "Unknown exception processing queued command",
                cmd.action_type
            ));
        }
    }

    LOG_INFO("Command queue processor thread exited (tid: {})",
             threadIdToString(std::this_thread::get_id()));
}

// ========== 缺失函数：执行策略判断 ==========
CommandDispatcher::ExecutionStrategy CommandDispatcher::determineStrategy(
    ServicePtr service,
    const std::shared_ptr<command::CommandContext>& context) const {

    // 1. 队列未启用 → 直接执行
    if (!enable_queue_) {
        return ExecutionStrategy::DIRECT;
    }

    // 2. 服务忙 → 入队（如果队列未满）
    if (service->isBusy()) {
        return (getQueueSize() < max_queue_size_)
            ? ExecutionStrategy::QUEUED
            : ExecutionStrategy::REJECTED;
    }

    // 3. 高优先级命令 → 直接执行
    auto priority = service->getPriority();
    if (priority == command::CommandPriority::HIGH ||
        priority == command::CommandPriority::CRITICAL) {
        return ExecutionStrategy::DIRECT;
    }

    // 4. 默认策略：入队（异步处理）
    return ExecutionStrategy::QUEUED;
}

    common::Result CommandDispatcher::executeImmediately(
        ServicePtr service,
        const device::ControlCommand& cmd,
        std::shared_ptr<command::CommandContext> context) {

        // 验证服务状态
        auto status = service->getStatus();
        if (status != "READY" && status != "RUNNING") {
            LOG_ERROR("Service: {} not ready for immediate execution, status: {}",
                      service->getName(), status);
            return common::Result::failure(
                common::command::EXECUTOR_RESOURCE_BUSY,
                "Service not ready",
                "Status: " + status
            );
        }

        try {
            LOG_INFO("Executing command immediately: {} to service {}",
                     cmd.cmd(), service->getName());
            auto result = service->execute(cmd, context);

            // 记录执行耗时
            result.setExecutionTime(context->getElapsedMicroseconds());
            return result;

        } catch (const std::exception& e) {
            LOG_ERROR("Exception during immediate command execution: {}", e.what());
            return common::Result::failure(
                common::command::EXECUTOR_FAILED,
                "Command execution failed",
                e.what()
            );
        }
    }

} // namespace protocol
} // namespace swan