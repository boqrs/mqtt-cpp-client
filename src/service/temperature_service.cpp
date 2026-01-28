//
// Created by wave on 2026/1/23.
//

#include <chrono>
#include <thread>
#include <algorithm>

#include "service/temperature_service.h"
#include "../../include/service/base/service_factory.h"
#include "logger/logger.h"
#include "utils/result.h"

namespace swan {
namespace services {

TemperatureService::TemperatureService()
    : BaseService({
        .name = "TemperatureService",
        .version = "1.0.0",
        .description = "Controls the 3D printer's temperature system",
        .action_type = "temperature_control",
        .execution_mode = command::ExecutionMode::ASYNC,
        .priority = command::CommandPriority::HIGH,
        .timeout_ms = 30000,  // 温度控制可能需要更长时间
        .max_concurrent = 1,
        .require_ack = true,
        .dependencies = {"hardware_service", "thermal_service"}
    }) {

    LOG_DEBUG("TemperatureService created");
}

std::vector<std::string> TemperatureService::getSupportedCommands() const {
    return {"temperature_control"};
}

common::Result TemperatureService::validateCommand(
    const swan::protocol::ControlCommand& cmd) const {

    if (cmd.cmd() != "temperature_control") {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for TemperatureService",
            "Expected: temperature_control, Got: " + cmd.cmd()
        );
    }

    if (!cmd.has_temperature_control()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing temperature_control data"
        );
    }

    const auto& temp_cmd = cmd.temperature_control();

    // 验证温度范围
    auto result = validateTemperature(temp_cmd.platform(), "platform");
    if (!result.isSuccess()) return result;

    result = validateTemperature(temp_cmd.right_nozzle(), "right_nozzle");
    if (!result.isSuccess()) return result;

    result = validateTemperature(temp_cmd.left_nozzle(), "left_nozzle");
    if (!result.isSuccess()) return result;

    result = validateTemperature(temp_cmd.chamber(), "chamber");
    if (!result.isSuccess()) return result;

    return common::Result::success("Command validation passed");
}

common::Result TemperatureService::validateTemperature(int32_t temp, const std::string& name) const{
    // 温度范围验证
    const int32_t MIN_TEMP = 0;
    const int32_t MAX_TEMP = 300;  // 假设最高300°C

    if (temp < MIN_TEMP || temp > MAX_TEMP) {
        return common::Result::failure(
            common::service::TEMP_OUT_OF_RANGE,
            "Temperature out of range for " + name,
            "Range: " + std::to_string(MIN_TEMP) + "-" + std::to_string(MAX_TEMP) +
            ", Got: " + std::to_string(temp)
        );
    }

    return common::Result::success();
}

common::Result TemperatureService::doExecute(
    const swan::protocol::ControlCommand& cmd,
    const std::shared_ptr<command::CommandContext>& context) {

    const auto& temp_cmd = cmd.temperature_control();

    LOG_INFO("Executing temperature control command - "
             "Platform: {}, RightNozzle: {}, LeftNozzle: {}, Chamber: {}",
             temp_cmd.platform(),
             temp_cmd.right_nozzle(),
             temp_cmd.left_nozzle(),
             temp_cmd.chamber());

    return setTemperatures(temp_cmd);
}

common::Result TemperatureService::setTemperatures(const protocol::TemperatureControlCmd& temp_cmd) {
    try {
        std::lock_guard<std::mutex> lock(state_mutex_);

        // 更新目标温度
        current_state_.platform_target = temp_cmd.platform();
        current_state_.right_nozzle_target = temp_cmd.right_nozzle();
        current_state_.left_nozzle_target = temp_cmd.left_nozzle();
        current_state_.chamber_target = temp_cmd.chamber();

        LOG_INFO("Setting target temperatures - "
                 "Platform: {}, RightNozzle: {}, LeftNozzle: {}, Chamber: {}",
                 current_state_.platform_target,
                 current_state_.right_nozzle_target,
                 current_state_.left_nozzle_target,
                 current_state_.chamber_target);

        // 启动异步加热过程
        std::thread([this]() {
            // 模拟加热过程
            bool all_reached = false;
            int attempts = 0;
            const int MAX_ATTEMPTS = 100;

            while (!all_reached && attempts < MAX_ATTEMPTS) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                attempts++;

                std::lock_guard<std::mutex> lock(state_mutex_);

                // 模拟温度变化
                auto update_result = updateTemperature(
                    current_state_.platform_temp,
                    current_state_.platform_target,
                    "platform"
                );

                update_result = updateTemperature(
                    current_state_.right_nozzle_temp,
                    current_state_.right_nozzle_target,
                    "right_nozzle"
                );

                update_result = updateTemperature(
                    current_state_.left_nozzle_temp,
                    current_state_.left_nozzle_target,
                    "left_nozzle"
                );

                update_result = updateTemperature(
                    current_state_.chamber_temp,
                    current_state_.chamber_target,
                    "chamber"
                );

                // 检查是否所有温度都达到目标
                all_reached =
                    std::abs(current_state_.platform_temp - current_state_.platform_target) <= 5 &&
                    std::abs(current_state_.right_nozzle_temp - current_state_.right_nozzle_target) <= 5 &&
                    std::abs(current_state_.left_nozzle_temp - current_state_.left_nozzle_target) <= 5 &&
                    std::abs(current_state_.chamber_temp - current_state_.chamber_target) <= 5;

                if (attempts % 10 == 0) {
                    LOG_DEBUG("Heating progress - Attempt: {}, "
                             "Platform: {}/{}, RightNozzle: {}/{}, "
                             "LeftNozzle: {}/{}, Chamber: {}/{}",
                             attempts,
                             current_state_.platform_temp, current_state_.platform_target,
                             current_state_.right_nozzle_temp, current_state_.right_nozzle_target,
                             current_state_.left_nozzle_temp, current_state_.left_nozzle_target,
                             current_state_.chamber_temp, current_state_.chamber_target);
                }
            }

            if (all_reached) {
                LOG_INFO("All temperatures reached target");
            } else {
                LOG_WARN("Temperature stabilization timed out");
            }
        }).detach();

        return common::Result::success("Temperature control started");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set temperatures: {}", e.what());
        return common::Result::failure(
            common::service::TEMP_HEATER_FAILED,
            "Failed to set temperatures",
            e.what()
        );
    }
}

common::Result TemperatureService::updateTemperature(int32_t& current,
                                                     int32_t target,
                                                     const std::string& name) {
    try {
        if (current < target) {
            // 加热
            int32_t increment = std::min(target - current, 5);  // 每次最多增加5°C
            current += increment;


        } else if (current > target) {
            // 冷却
            int32_t decrement = std::min(current - target, 3);  // 每次最多减少3°C
            current -= decrement;
        }

        return common::Result::success();

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::service::TEMP_SENSOR_FAILED,
            "Failed to update temperature for " + name,
            e.what()
        );
    }
}

} // namespace services
} // namespace swan
