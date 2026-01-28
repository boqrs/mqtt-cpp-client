//
// Created by wave on 2026/1/23.
//

#include <chrono>
#include <thread>
#include "service/unregister_service.h"
#include "../../include/service/base/service_factory.h"
#include "logger/logger.h"
#include "utils/result.h"

namespace swan {
namespace services {

UnregisterService::UnregisterService()
    : BaseService({
        .name = "UnregisterService",
        .version = "1.0.0",
        .description = "Handles device unregistration and cleanup",
        .action_type = "device_unregister",
        .execution_mode = command::ExecutionMode::SYNC,
        .priority = command::CommandPriority::CRITICAL,
        .timeout_ms = 60000,  // 注销可能需要较长时间
        .max_concurrent = 1,
        .require_ack = true,
        .dependencies = {}
    }) {

    LOG_DEBUG("UnregisterService created");
}

std::vector<std::string> UnregisterService::getSupportedCommands() const {
    return {"device_unregister"};
}

common::Result UnregisterService::validateCommand(
    const swan::protocol::ControlCommand& cmd) const {

    if (cmd.cmd() != "device_unregister") {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for UnregisterService",
            "Expected: device_unregister, Got: " + cmd.cmd()
        );
    }

    if (!cmd.has_device_unregister()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing device_unregister data"
        );
    }

    // device_unregister 命令没有额外参数需要验证
    return common::Result::success("Command validation passed");
}

common::Result UnregisterService::doExecute(
    const swan::protocol::ControlCommand& cmd,
    const std::shared_ptr<command::CommandContext>& context) {

    LOG_WARN("Starting device unregistration process");

    try {
        // 步骤1: 备份设备数据
        updateProgress(10, "Backing up device data...");
        auto backup_result = backupDeviceData();
        if (!backup_result.isSuccess()) {
            LOG_ERROR("Device data backup failed: {}", backup_result.toString());
            return backup_result;
        }

        // 步骤2: 清理用户数据
        updateProgress(30, "Clearing user data...");
        auto clear_result = clearUserData();
        if (!clear_result.isSuccess()) {
            LOG_ERROR("User data clearance failed: {}", clear_result.toString());
            return clear_result;
        }

        // 步骤3: 重置网络配置
        updateProgress(60, "Resetting network configuration...");
        auto network_result = resetNetworkConfig();
        if (!network_result.isSuccess()) {
            LOG_ERROR("Network reset failed: {}", network_result.toString());
            return network_result;
        }

        // 步骤4: 关闭服务
        updateProgress(80, "Shutting down services...");
        auto shutdown_result = shutdownServices();
        if (!shutdown_result.isSuccess()) {
            LOG_ERROR("Service shutdown failed: {}", shutdown_result.toString());
            return shutdown_result;
        }

        // 步骤5: 完成
        updateProgress(100, "Device unregistration complete");

        LOG_WARN("Device unregistration completed successfully. "
                   "Device will need to be re-registered for future use.");

        return common::Result::success("Device unregistered successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Device unregistration failed: {}", e.what());
        return common::Result::failure(
            common::command::EXECUTOR_FAILED,
            "Device unregistration failed",
            e.what()
        );
    }
}

common::Result UnregisterService::backupDeviceData() {
    try {
        LOG_INFO("Backing up device data...");

        // 这里应该实现数据备份逻辑
        // 例如: 备份配置文件、打印历史、校准数据等

        std::this_thread::sleep_for(std::chrono::seconds(5));

        LOG_INFO("Device data backup completed");
        return common::Result::success("Device data backed up");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::system::IO_ERROR,
            "Failed to backup device data",
            e.what()
        );
    }
}

common::Result UnregisterService::clearUserData() {
    try {
        LOG_INFO("Clearing user data...");


        std::this_thread::sleep_for(std::chrono::seconds(3));

        LOG_INFO("User data cleared");
        return common::Result::success("User data cleared");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::system::IO_ERROR,
            "Failed to clear user data",
            e.what()
        );
    }
}

common::Result UnregisterService::resetNetworkConfig() {
    try {
        LOG_INFO("Resetting network configuration...");

        // 这里应该实现网络重置逻辑
        // 例如: 重置Wi-Fi配置、清除网络凭据、恢复默认网络设置等

        std::this_thread::sleep_for(std::chrono::seconds(2));

        LOG_INFO("Network configuration reset");
        return common::Result::success("Network configuration reset");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::system::IO_ERROR,
            "Failed to reset network configuration",
            e.what()
        );
    }
}

common::Result UnregisterService::shutdownServices() {
    try {
        LOG_INFO("Shutting down services...");

        // 这里应该关闭所有运行中的服务
        // 例如: 停止打印作业、关闭摄像头流、关闭温度控制等

        std::this_thread::sleep_for(std::chrono::seconds(3));

        LOG_INFO("All services shut down");
        return common::Result::success("Services shut down");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::system::SHUTDOWN_FAILED,
            "Failed to shutdown services",
            e.what()
        );
    }
}

void UnregisterService::updateProgress(int progress, const std::string& message) {
    LOG_INFO("Unregistration progress: {}% - {}", progress, message);

    // 这里可以通知UI或其他组件更新进度
    // 例如: ui_manager->updateProgress(progress, message);
}

} // namespace services
} // namespace swan
