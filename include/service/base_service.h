//
// Created by wave on 2026/1/23.
//
// base_service.h
// base_service.h 修复 override
#pragma once

#include "protocol.pb.h"
#include "utils/result.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

#include "protocol/command/cmd.h"
#include "utils/models/protocol.h"

namespace swan {
namespace services {

// 使用 protobuf 生成的 ControlCommand
using ControlCommand = ::swan::device::ControlCommand;

// 服务状态
enum class ServiceStatus {
    UNINITIALIZED = 0,
    INITIALIZING,
    READY,
    RUNNING,
    PAUSED,
    STOPPING,
    ERROR,
    SHUTDOWN
};

// 服务配置
struct ServiceConfig {
    std::string name;
    std::string version;
    std::string description;
    std::string action_type;
    swan::protocol::command::ExecutionMode execution_mode;
    swan::protocol::command::CommandPriority priority;
    uint32_t timeout_ms;
    uint32_t max_concurrent;
    bool require_ack;
    std::vector<std::string> dependencies;
};

// 服务基类
class BaseService : public protocol::command::ICommandHandler {
public:
    explicit BaseService(const ServiceConfig& config);
    virtual ~BaseService();

    // ICommandHandler实现
    std::string getName() const override { return config_.name; }
    std::string getVersion() const override { return config_.version; }
    std::string getDescription() const override { return config_.description; }
    std::string getSupportedActionType() const override { return config_.action_type; }

    protocol::command::ExecutionMode getExecutionMode() const override {
        return config_.execution_mode;
    }

    protocol::command::CommandPriority getPriority() const override {
        return config_.priority;
    }

    uint32_t getTimeoutMs() const override {
        return config_.timeout_ms;
    }

    // 修改为返回 std::string 以匹配接口
    std::string getStatus() const override {
        return getStatusString();
    }

    // 状态管理
    ServiceStatus getServiceStatus() const { return status_; }
    std::string getStatusString() const;

    // 生命周期管理 - 修正为返回 bool
    bool initialize() override;
    bool shutdown() override;

    // 统计信息
    struct Statistics {
        uint64_t total_executions = 0;
        uint64_t successful_executions = 0;
        uint64_t failed_executions = 0;
        uint64_t total_execution_time_us = 0;
        uint64_t last_execution_time_us = 0;

        void reset() {
            total_executions = 0;
            successful_executions = 0;
            failed_executions = 0;
            total_execution_time_us = 0;
            last_execution_time_us = 0;
        }

        double getAverageExecutionTimeMs() const {
            if (total_executions == 0) return 0.0;
            return static_cast<double>(total_execution_time_us) / total_executions / 1000.0;
        }
    };

    Statistics getStatistics() const { return statistics_; }

    // 命令支持 - 添加 override
    virtual std::vector<std::string> getSupportedCommands() const override {
        return {config_.action_type};
    }

    // 验证命令参数 - 添加 override
    virtual common::Result validateCommand(
        const ControlCommand& cmd) const override {
        return common::Result::success();
    }

    // 执行方法
    common::Result execute(
        const ControlCommand& cmd,
        const std::shared_ptr<protocol::command::CommandContext>& context) override;

    // 状态查询
    bool isBusy() const override { return false; }

protected:
    // 状态转换
    bool setStatus(ServiceStatus new_status, const std::string& message = "");

    // 统计更新
    void updateStatistics(const common::Result& result, uint64_t execution_time_us);

    // 配置访问
    const ServiceConfig& getConfig() const { return config_; }

    // 执行包装器
    common::Result executeWithGuard(
        const ControlCommand& cmd,
        const std::shared_ptr<protocol::command::CommandContext>& context);

    // 纯虚函数，子类必须实现
    virtual common::Result doExecute(
        const ControlCommand& cmd,
        const std::shared_ptr<protocol::command::CommandContext>& context) = 0;

    // 服务特定的初始化和关闭
    virtual common::Result onInitialize() { return common::Result::success(); }
    virtual common::Result onShutdown() { return common::Result::success(); }

private:
    ServiceConfig config_;
    std::atomic<ServiceStatus> status_;
    std::atomic<Statistics> statistics_;
    mutable std::mutex status_mutex_;
};

} // namespace services
} // namespace swan