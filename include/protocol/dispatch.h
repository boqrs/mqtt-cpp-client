//
// Created by wave on 2026/1/23.
//

#pragma once

#include <memory>
#include <string>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <unordered_map>
#include "protocol.pb.h"
#include "utils/result.h"
#include "protocol/command/context.h"
#include "../service/base/service_factory.h"

namespace swan {
namespace prodispatcher {

class CommandDispatcher {
public:
    using ServicePtr = std::shared_ptr<services::BaseService>;

    CommandDispatcher();
    ~CommandDispatcher();

    // 协议解析
    common::Result dispatchUnifiedMessage(
        const protocol::UnifiedMessage& message,
        const std::shared_ptr<swan::command::CommandContext> context);

    //命令执行
    common::Result dispatchControlCommand(
        const protocol::ControlCommand& cmd,
        const std::string& action_type,
        const std::shared_ptr<swan::command::CommandContext>& context);

    // 服务管理
    bool registerService(ServicePtr service);
    bool unregisterService(const std::string& action_type);
    ServicePtr getService(const std::string& action_type) const;

    // 执行策略
    enum class ExecutionStrategy {
        DIRECT,
        QUEUED,
        SCHEDULED,
        REJECTED
    };

    // 队列管理
    struct QueuedCommand {
        protocol::ControlCommand cmd;
        std::shared_ptr<command::CommandContext> context;
        std::string action_type;
        std::chrono::steady_clock::time_point enqueue_time;
    };

    // 状态查询
    size_t getQueueSize() const;
    // 配置
    void setMaxQueueSize(size_t size) { max_queue_size_ = size; }
    size_t getMaxQueueSize() const { return max_queue_size_; }

    void setEnableQueue(bool enable) { enable_queue_ = enable; }
    bool getEnableQueue() const { return enable_queue_; }

private:
    // 服务查找
    ServicePtr findServiceForAction(const std::string& action_type);

    // 执行方法
    common::Result executeImmediately(
        ServicePtr service,
        const protocol::ControlCommand& cmd,
        std::shared_ptr<command::CommandContext> context);

    // 队列处理
    bool enqueueCommand(QueuedCommand&& command);
    std::optional<QueuedCommand> dequeueCommand();
    void queueProcessorThread();

    // 执行策略决定
    ExecutionStrategy determineStrategy(
        ServicePtr service,
        const std::shared_ptr<command::CommandContext>& context) const;

    std::unordered_map<std::string, ServicePtr> services_;
    mutable std::mutex services_mutex_;  // 改为 std::mutex

    std::queue<QueuedCommand> command_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::thread queue_processor_;
    std::atomic<bool> queue_running_{false};

    size_t max_queue_size_ = 1000;
    bool enable_queue_ = true;

    // 服务工厂引用
    services::ServiceFactory& service_factory_;
};

} // namespace protocol
} // namespace swan