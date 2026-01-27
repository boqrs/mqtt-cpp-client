//
// Created by wave on 2026/1/23.
//


#include <regex>

#include "service/profile_service.h"
#include "service/service_factory.h"
#include "logger/logger.h"
#include "utils/result.h"

namespace swan {
namespace services {

UserService::UserService()
    : BaseService({
        .name = "UserService",
        .version = "1.0.0",
        .description = "Manages user profile information",
        .action_type = "user_profile",
        .execution_mode = protocol::command::ExecutionMode::SYNC,
        .priority = protocol::command::CommandPriority::LOW,
        .timeout_ms = 3000,
        .max_concurrent = 1,
        .require_ack = false,  // 用户资料更新不需要确认
        .dependencies = {}
    }) {

    // 初始化默认用户资料
    current_profile_ = {
        .avatar_url = "",
        .user_name = "Guest",
        .last_update = std::chrono::system_clock::now()
    };

    LOG_DEBUG("UserService created with default guest profile");
}

std::vector<std::string> UserService::getSupportedCommands() const {
    return {"user_profile"};
}

common::Result UserService::validateCommand(
    const swan::device::ControlCommand& cmd) const {

    if (cmd.cmd() != "user_profile") {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for UserService",
            "Expected: user_profile, Got: " + cmd.cmd()
        );
    }

    if (!cmd.has_user_profile()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing user_profile data"
        );
    }

    const auto& profile_cmd = cmd.user_profile();

    // 验证用户数据
    return validateUserProfile(profile_cmd);
}

common::Result UserService::validateUserProfile(const device::UserProfileCmd& profile_cmd) const{
    // 验证用户名
    if (profile_cmd.name().empty()) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "User name cannot be empty"
        );
    }

    if (profile_cmd.name().length() > 50) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "User name too long",
            "Max length: 50 characters"
        );
    }

    // 验证头像URL（如果提供）
    if (!profile_cmd.avatar().empty()) {
        // 简单的URL格式验证
        std::regex url_pattern(
            R"(^(http|https)://[a-zA-Z0-9\-\.]+\.[a-zA-Z]{2,}(/\S*)?$)"
        );

        if (!std::regex_match(profile_cmd.avatar(), url_pattern)) {
            return common::Result::failure(
                common::common::INVALID_PARAMETER,
                "Invalid avatar URL format"
            );
        }
    }

    return common::Result::success("User profile validation passed");
}

common::Result UserService::doExecute(
    const swan::device::ControlCommand& cmd,
    const std::shared_ptr<protocol::command::CommandContext>& context) {

    const auto& profile_cmd = cmd.user_profile();

    LOG_INFO("Updating user profile - Name: {}, Avatar: {}",
             profile_cmd.name(),
             profile_cmd.avatar().empty() ? "none" : "provided");

    try {
        std::lock_guard<std::mutex> lock(profile_mutex_);

        // 更新用户资料
        current_profile_.avatar_url = profile_cmd.avatar();
        current_profile_.user_name = profile_cmd.name();
        current_profile_.last_update = std::chrono::system_clock::now();

        // 更新显示
        auto display_result = updateUserDisplay();
        if (!display_result.isSuccess()) {
            LOG_WARN("Failed to update user display: {}", display_result.toString());
        }

        LOG_INFO("User profile updated successfully");

        return common::Result::success("User profile updated")
            .setData(current_profile_.user_name);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update user profile: {}", e.what());
        return common::Result::failure(
            common::command::EXECUTOR_FAILED,
            "Failed to update user profile",
            e.what()
        );
    }
}

common::Result UserService::updateUserDisplay() {
    try {
        LOG_DEBUG("Updating user display with name: {}", current_profile_.user_name);


        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return common::Result::success("User display updated");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::command::EXECUTOR_FAILED,
            "Failed to update display",
            e.what()
        );
    }
}

} // namespace services
} // namespace swan
