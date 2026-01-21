//
// Created by wave on 2026/1/21.
//

#pragma once
#include "command/cmd.h"
#include "command/result.h"
#include "service/temperature_service.h"
#include "protocol.pb.h"
#include <memory>

namespace swan {
namespace command {

class TemperatureControlHandler : public ICommandHandler {
public:
    explicit TemperatureControlHandler(std::shared_ptr<service::TemperatureService> tempService)
        : tempService_(std::move(tempService)) {}

    std::string getSupportedActionType() const override {
        return "temperature_control";
    }

    CommandResult execute(
        const device::ControlCommand& cmd,
        const std::shared_ptr<CommandContext>& context) override {

        // 快速参数验证
        if (!context || !cmd.has_temperature_control()) {
            return CommandResult::failure(
                ErrorCode::INVALID_PARAMETER,
                "Invalid temperature control command"
            );
        }

        const auto& tempCmd = cmd.temperature_control();
        const std::string& deviceId = context->getDeviceId();

        try {
            // 可选的温度范围验证
            constexpr int MIN_TEMP = 0;
            constexpr int MAX_TEMP = 300;

            if ((tempCmd.platform() > 0 && (tempCmd.platform() < MIN_TEMP || tempCmd.platform() > MAX_TEMP)) ||
                (tempCmd.right_nozzle() > 0 && (tempCmd.right_nozzle() < MIN_TEMP || tempCmd.right_nozzle() > MAX_TEMP)) ||
                (tempCmd.left_nozzle() > 0 && (tempCmd.left_nozzle() < MIN_TEMP || tempCmd.left_nozzle() > MAX_TEMP)) ||
                (tempCmd.chamber() > 0 && (tempCmd.chamber() < MIN_TEMP || tempCmd.chamber() > MAX_TEMP))) {
                return CommandResult::failure(
                    ErrorCode::INVALID_PARAMETER,
                    "Temperature out of valid range",
                    "Valid range: " + std::to_string(MIN_TEMP) + "-" + std::to_string(MAX_TEMP) + "°C"
                );
            }

            // 调用温度服务
           /* bool success = tempService_->setTemperatures(deviceId, {
                .platform = tempCmd.platform(),
                .rightNozzle = tempCmd.right_nozzle(),
                .leftNozzle = tempCmd.left_nozzle(),
                .chamber = tempCmd.chamber()
            });*/
            bool success = true;
            if (success) {
                return CommandResult::success("Temperature set successfully");
            } else {
                return CommandResult::failure(
                    ErrorCode::EXECUTION_FAILED,
                    "Failed to set temperatures"
                );
            }

        } catch (const std::exception& e) {
            return CommandResult::failure(
                ErrorCode::INTERNAL_ERROR,
                "Temperature control exception",
                e.what()
            );
        }
    }

private:
    std::shared_ptr<service::TemperatureService> tempService_;
};

} // namespace command
} // namespace swan