//
// Created by wave on 2026/1/21.
//
#pragma once
#include "command/cmd.h"
#include "command/result.h"
#include "service/light_service.h"
#include "protocol.pb.h"
#include <memory>

namespace swan {
namespace command {

/**
 * @brief 灯光控制处理器（纯头文件实现）
 * 负责处理 action_type = "light_control" 的指令
 */
class LightControlHandler : public ICommandHandler {
public:
    /**
     * @brief 构造函数
     * @param lightService 灯光业务服务实例
     */
    explicit LightControlHandler(std::shared_ptr<service::LightService> lightService)
        : lightService_(std::move(lightService)) {
        // 可以在这里添加初始化逻辑
    }

    /**
     * @brief 获取支持的 action_type
     * @return "light_control"
     */
    std::string getSupportedActionType() const override {
        return "light_control";
    }

    /**
     * @brief 执行灯光控制命令
     */
    CommandResult execute(
        const device::ControlCommand& cmd,
        const std::shared_ptr<CommandContext>& context) override {

        // 参数验证
        if (!context) {
            return CommandResult::failure(
                ErrorCode::INVALID_PARAMETER,
                "Command context is null"
            );
        }

        if (!cmd.has_light_control()) {
            return CommandResult::failure(
                ErrorCode::INVALID_PARAMETER,
                "Missing light_control field in command"
            );
        }

        const auto& lightCmd = cmd.light_control();
        const std::string& deviceId = context->getDeviceId();

        // 执行具体业务逻辑
        try {
            bool success = false;
            std::string operation;

            switch (lightCmd.status()) {
                case device::LightControlCmd::OPEN:
                    success = lightService_->turnOn(deviceId);
                    operation = "turn on";
                    break;

                case device::LightControlCmd::CLOSE:
                    success = lightService_->turnOff(deviceId);
                    operation = "turn off";
                    break;

                default:
                    return CommandResult::failure(
                        ErrorCode::INVALID_PARAMETER,
                        "Unknown light status",
                        "Status value: " + std::to_string(lightCmd.status())
                    );
            }

            if (success) {
                return CommandResult::success(
                    "Light " + operation + " successfully for device: " + deviceId
                );
            } else {
                return CommandResult::failure(
                    ErrorCode::EXECUTION_FAILED,
                    "Failed to " + operation + " light",
                    "Device: " + deviceId
                );
            }

        } catch (const std::exception& e) {
            return CommandResult::failure(
                ErrorCode::INTERNAL_ERROR,
                "Exception in light control",
                e.what()
            );
        }
    }

    /**
     * @brief 异步执行（可选重写）
     */
    std::future<CommandResult> executeAsync(
        const device::ControlCommand& cmd,
        const std::shared_ptr<CommandContext>& context) override {

        // 使用默认实现，在单独线程中执行
        return std::async(std::launch::async, [this, &cmd, context]() {
            return execute(cmd, context);
        });
    }

private:
    std::shared_ptr<service::LightService> lightService_;
};

} // namespace command
} // namespace swan