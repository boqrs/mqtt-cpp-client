//
// Created by wave on 2026/1/23.
//

#include <chrono>
#include <random>
#include "service/stream_service.h"
#include "service/service_factory.h"
#include "logger/logger.h"
#include "utils/result.h"

namespace swan {
namespace services {

StreamService::StreamService()
    : BaseService({
        .name = "StreamService",
        .version = "1.0.0",
        .description = "Controls the 3D printer's video and data streaming",
        .action_type = "stream_control",
        .execution_mode = protocol::command::ExecutionMode::ASYNC,
        .priority = protocol::command::CommandPriority::NORMAL,
        .timeout_ms = 10000,
        .max_concurrent = 5,  // 支持多个并发流
        .require_ack = true,
        .dependencies = {"hardware_service", "camera_service"}
    }) {

    LOG_DEBUG("StreamService created");
}

StreamService::~StreamService() {
    // 关闭所有活动会话
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [session_id, session] : active_sessions_) {
        if (session.active) {
            LOG_WARN("Stream session {} still active during destruction", session_id);
            stopCameraStream(session_id);
        }
    }
    active_sessions_.clear();
}

std::vector<std::string> StreamService::getSupportedCommands() const {
    return {"stream_control"};
}

common::Result StreamService::validateCommand(
    const swan::device::ControlCommand& cmd) const {

    if (cmd.cmd() != "stream_control") {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for StreamService",
            "Expected: stream_control, Got: " + cmd.cmd()
        );
    }

    if (!cmd.has_stream_control()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing stream_control data"
        );
    }

    const auto& stream_cmd = cmd.stream_control();

    if (stream_cmd.action() == device::StreamControlCmd_StreamAction_UNKNOWN_ACTION) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Invalid stream action: UNKNOWN_ACTION"
        );
    }

    return common::Result::success("Command validation passed");
}

common::Result StreamService::doExecute(
    const swan::device::ControlCommand& cmd,
    const std::shared_ptr<protocol::command::CommandContext>& context) {

    const auto& stream_cmd = cmd.stream_control();

    LOG_INFO("Executing stream control command, action: {}",
             device::StreamControlCmd_StreamAction_Name(stream_cmd.action()));

    switch (stream_cmd.action()) {
        case device::StreamControlCmd_StreamAction_OPEN:
            return startStream(stream_cmd, context);

        case device::StreamControlCmd_StreamAction_CLOSE:
            return stopStream(stream_cmd, context);

        case device::StreamControlCmd_StreamAction_UNKNOWN_ACTION:
        default:
            return common::Result::failure(
                common::common::INVALID_PARAMETER,
                "Unknown stream action"
            );
    }
}

common::Result StreamService::startStream(const device::StreamControlCmd& stream_cmd,
                                         const std::shared_ptr<protocol::command::CommandContext>& context) {

    try {
        std::lock_guard<std::mutex> lock(sessions_mutex_);

        // 检查是否有太多活动会话
        if (active_sessions_.size() >= static_cast<size_t>(getConfig().max_concurrent)) {
            return common::Result::failure(
                common::service::STREAM_SESSION_NOT_FOUND,
                "Maximum concurrent streams reached",
                "Max: " + std::to_string(getConfig().max_concurrent)
            );
        }

        // 生成会话ID
        std::string session_id = generateSessionId();

        // 启动硬件流
        auto hardware_result = startCameraStream(session_id);
        if (!hardware_result.isSuccess()) {
            return hardware_result;
        }

        // 创建并插入会话 - 使用 emplace 直接构造
        auto [it, inserted] = active_sessions_.emplace(
            session_id,
            StreamSession(session_id,
                         context->getProperty("client_ip", "unknown"),
                         std::chrono::steady_clock::now(),
                         true)
        );

        if (!inserted) {
            LOG_ERROR("Failed to create stream session {}", session_id);
            return common::Result::failure(
                common::service::STREAM_SESSION_NOT_FOUND,
                "Failed to create stream session"
            );
        }

        LOG_INFO("Stream session {} started for client {}",
                 session_id, it->second.client_ip);

        // 返回会话信息
        return common::Result::success("Stream started successfully")
            .setData(session_id);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start stream: {}", e.what());
        return common::Result::failure(
            common::service::STREAM_CAMERA_FAILED,
            "Failed to start stream",
            e.what()
        );
    }
}

common::Result StreamService::stopStream(const device::StreamControlCmd& stream_cmd,
                                        const std::shared_ptr<protocol::command::CommandContext>& context) {

    try {
        std::lock_guard<std::mutex> lock(sessions_mutex_);

        // 获取会话ID（可以从context中获取）
        std::string session_id = context->getProperty("session_id", "");

        if (session_id.empty()) {
            // 如果没有指定会话ID，停止所有会话
            size_t stopped_count = 0;
            for (auto& [sid, session] : active_sessions_) {
                if (session.active) {
                    auto result = stopCameraStream(sid);
                    if (result.isSuccess()) {
                        session.active = false;
                        stopped_count++;
                    }
                }
            }

            LOG_INFO("Stopped {} stream sessions", stopped_count);
            return common::Result::success("Stopped all stream sessions");
        }

        // 停止指定会话
        auto it = active_sessions_.find(session_id);
        if (it == active_sessions_.end() || !it->second.active) {
            return common::Result::failure(
                common::service::STREAM_SESSION_NOT_FOUND,
                "Stream session not found or already stopped",
                "Session ID: " + session_id
            );
        }

        // 停止硬件流
        auto hardware_result = stopCameraStream(session_id);
        if (!hardware_result.isSuccess()) {
            return hardware_result;
        }

        // 更新会话状态
        it->second.active = false;

        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - it->second.start_time);

        LOG_INFO("Stream session {} stopped, duration: {} seconds",
                 session_id, duration.count());

        return common::Result::success("Stream stopped successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to stop stream: {}", e.what());
        return common::Result::failure(
            common::service::STREAM_CAMERA_FAILED,
            "Failed to stop stream",
            e.what()
        );
    }
}

std::string StreamService::generateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    int id_num = session_counter_.fetch_add(1) + dis(gen);
    return "STREAM_" + std::to_string(id_num);
}

common::Result StreamService::startCameraStream(const std::string& session_id) {
    try {
        LOG_DEBUG("Starting camera stream for session {}", session_id);

        // 模拟硬件启动
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 这里应该调用实际的摄像头硬件接口
        // 例如: camera_manager->startStream(session_id);

        LOG_INFO("Camera stream started for session {}", session_id);
        return common::Result::success("Camera stream started");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::service::STREAM_CAMERA_FAILED,
            "Failed to start camera stream",
            e.what()
        );
    }
}

common::Result StreamService::stopCameraStream(const std::string& session_id) {
    try {
        LOG_DEBUG("Stopping camera stream for session {}", session_id);

        // 模拟硬件停止
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 这里应该调用实际的摄像头硬件接口
        // 例如: camera_manager->stopStream(session_id);

        LOG_INFO("Camera stream stopped for session {}", session_id);
        return common::Result::success("Camera stream stopped");

    } catch (const std::exception& e) {
        return common::Result::failure(
            common::service::STREAM_CAMERA_FAILED,
            "Failed to stop camera stream",
            e.what()
        );
    }
}

} // namespace services
} // namespace swan

namespace {
    // 使用匿名命名空间确保唯一性
    static swan::services::ServiceRegistrar<swan::services::StreamService>
        _light_service_registrar("stream_control");
}
