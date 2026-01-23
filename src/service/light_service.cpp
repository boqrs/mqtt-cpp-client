//
// Created by wave on 2026/1/23.
//

// services/light_service.cpp
#include <chrono>
#include <thread>

#include "logger/logger.h"
#include "service/light_service.h"
#include "utils/result.h"
#include "service/service_factory.h"

namespace swan {
namespace services {
LightService::LightService()
    : BaseService({
        .name = "LightService",
        .version = "1.0.0",
        .description = "Controls the 3D printer's lighting system",
        .action_type = "light_control",
        .execution_mode = protocol::command::ExecutionMode::SYNC,
        .priority = protocol::command::CommandPriority::NORMAL,
        .timeout_ms = 5000,
        .max_concurrent = 1,
        .require_ack = true,
        .dependencies = {"hardware_service"}
    }) {

    LOG_DEBUG("LightService created");
}

std::vector<std::string> LightService::getSupportedCommands() const {
    return {"light_control"};
}

common::Result LightService::validateCommand(
    const swan::device::ControlCommand& cmd) const {

    if (cmd.cmd() != "light_control") {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for LightService",
            "Expected: light_control, Got: " + cmd.cmd()
        );
    }

    if (!cmd.has_light_control()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing light_control data"
        );
    }

    const auto& light_cmd = cmd.light_control();

    // 验证状态值
    if (light_cmd.status() == device::LightControlCmd_LightStatus_UNKNOWN) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Invalid light status: UNKNOWN"
        );
    }

    return common::Result::success("Command validation passed");
}

common::Result LightService::doExecute(
    const swan::device::ControlCommand& cmd,
    const std::shared_ptr<protocol::command::CommandContext>& context) {

    const auto& light_cmd = cmd.light_control();

    LOG_INFO("Executing light control command, status: {}",
             device::LightControlCmd_LightStatus_Name(light_cmd.status()));

    switch (light_cmd.status()) {
        case device::LightControlCmd_LightStatus_OPEN:
            return turnOnLight();

        case device::LightControlCmd_LightStatus_CLOSE:
            return turnOffLight();

        case device::LightControlCmd_LightStatus_UNKNOWN:
        default:
            return common::Result::failure(
                common::common::INVALID_PARAMETER,
                "Unknown light status"
            );
    }
}

common::Result LightService::turnOnLight() {
    if (light_on_.load()) {
        return common::Result::failure(
            common::service::LIGHT_ALREADY_ON,
            "Light is already on"
        );
    }

    try {
        LOG_INFO("Turning on light...");

        // 模拟硬件操作
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        light_on_.store(true);

        LOG_INFO("Light turned on successfully");
        return common::Result::success("Light turned on");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to turn on light: {}", e.what());
        return common::Result::failure(
            common::service::LIGHT_HARDWARE_FAILED,
            "Failed to turn on light",
            e.what()
        );
    }
}

common::Result LightService::turnOffLight() {
    if (!light_on_.load()) {
        return common::Result::failure(
            common::service::LIGHT_ALREADY_OFF,
            "Light is already off"
        );
    }

    try {
        LOG_INFO("Turning off light...");

        // 模拟硬件操作
        std::this_thread::sleep_for(std::chrono::milliseconds(100));


        light_on_.store(false);

        LOG_INFO("Light turned off successfully");
        return common::Result::success("Light turned off");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to turn off light: {}", e.what());
        return common::Result::failure(
            common::service::LIGHT_HARDWARE_FAILED,
            "Failed to turn off light",
            e.what()
        );
    }
}

common::Result LightService::getLightStatus() {
    bool status = light_on_.load();

    std::string status_str = status ? "ON" : "OFF";
    LOG_DEBUG("Current light status: {}", status_str);

    return common::Result::success("Light status retrieved")
        .setData(status_str);
}

} // namespace services
} // namespace swan

namespace {
    // 使用匿名命名空间确保唯一性
    static swan::services::ServiceRegistrar<swan::services::LightService>
        _light_service_registrar("light_service");
}
