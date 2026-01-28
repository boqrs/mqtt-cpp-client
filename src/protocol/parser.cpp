//
// Created by wave on 2026/1/23.
//


#include <google/protobuf/util/json_util.h>
#include <chrono>
#include <regex>
#include <mutex>

#include "protocol/command/context.h"
#include "protocol/parser.h"
#include "logger/logger.h"

namespace swan {
namespace proparser {

// ==================== ProtocolParser 实现 ====================

ProtocolParser::ProtocolParser()
    : protocol_version_({1, 0, 0})
    , strict_mode_(true) {

    LOG_DEBUG("ProtocolParser initialized with version: {}",
              protocol_version_.toString());
}

ProtocolParser::~ProtocolParser() {
    // 析构函数，如有需要清理的资源可以在这里处理
}

common::Result ProtocolParser::parseAndHandle(
    std::string_view raw_data,
    Callback response_callback) {

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // 步骤1: 解析消息
        protocol::UnifiedMessage message;
        auto parse_result = parseMessage(raw_data, message);

        if (!parse_result.isSuccess()) {
            recordMessage(false, false, 0);
            LOG_ERROR("Error: Message parsing failed: {}", parse_result.toString());
            return parse_result;
        }

        // 步骤2: 验证消息
        auto validation_result = validateMessage(message);
        if (!validation_result.isSuccess()) {
            recordMessage(false, true, 0);
            LOG_ERROR("Warning: Message validation failed: {}", validation_result.toString());
            if (strict_mode_) {
                return validation_result;
            }
            // 非严格模式下，记录警告但继续处理
            LOG_INFO("Debug: Continuing message processing in non-strict mode");
        }

        // 步骤3: 根据消息类型处理
        common::Result handle_result;
        std::string message_type = message.message_type();

        if (message_type == "device_cmd") {
            // 设备端接收到的下行命令
            LOG_INFO("Debug: Processing device command: {}", message.name());

            handle_result = handleDeviceCommand(message, response_callback);

        } else if (message_type == "device_state") {
            LOG_ERROR("Warning: Device received device_state message, which is unusual");

            handle_result = handleDeviceState(message, response_callback);

        } else {
            LOG_ERROR("Error: Unknown message type: {}", message_type);

            recordMessage(false, true, 0);

            return common::Result::failure(
                common::protocol::PARSER_UNSUPPORTED_MSG_TYPE,
                "Unknown message type: " + message_type
            );
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto parse_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();

        // 记录成功消息
        recordMessage(true, false, parse_time_ns);

        // 设置数据并返回
        handle_result.setData(message);
        return handle_result;

    } catch (const std::exception& e) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto parse_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();

        recordMessage(false, false, parse_time_ns);

        LOG_ERROR("Error: Exception during message processing: {}", e.what() );
        return common::Result::failure(
            common::protocol::PARSER_DESERIALIZATION_FAILED,
            "Exception during message processing",
            e.what()
        );
    }
}

void ProtocolParser::setProtocolVersion(const ProtocolVersion& version) {
    LOG_INFO("Setting protocol version to: {}", version.toString());
    protocol_version_ = version;
}

ProtocolVersion ProtocolParser::getProtocolVersion() const {
    return protocol_version_;
}

ProtocolParser::Statistics ProtocolParser::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

void ProtocolParser::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    LOG_DEBUG("Resetting parser statistics");
    statistics_.reset();
}

// ==================== 私有方法实现 ====================

common::Result ProtocolParser::parseMessage(
    std::string_view raw_data,
    protocol::UnifiedMessage& message) {

    if (raw_data.empty()) {
        LOG_WARN("Empty raw data received");
        return common::Result::failure(
            common::protocol::PARSER_MESSAGE_TOO_SMALL,
            "Empty message data"
        );
    }

    // 方法1: 尝试二进制解析 (Protobuf 格式)
    bool parse_success = false;

    // 首先尝试二进制解析，假设数据是 Protobuf 二进制格式
    parse_success = message.ParseFromArray(raw_data.data(),
                                          static_cast<int>(raw_data.size()));

    if (parse_success) {
        LOG_DEBUG("Successfully parsed message as binary Protobuf");
        return common::Result::success("Parsed as binary Protobuf");
    }

    // 方法2: 尝试 JSON 解析
    try {
        google::protobuf::util::JsonParseOptions options;
        options.ignore_unknown_fields = !strict_mode_;

        std::string json_str(raw_data.data(), raw_data.size());
        auto status = google::protobuf::util::JsonStringToMessage(json_str,
                                                                 &message,
                                                                 options);

        if (status.ok()) {
            LOG_DEBUG("Successfully parsed message as JSON");
            return common::Result::success("Parsed as JSON");
        }

        LOG_DEBUG("JSON parsing failed: {}", status.ToString());

    } catch (const std::exception& e) {
        LOG_DEBUG("Exception during JSON parsing: {}", e.what());
    }

    // 所有解析方法都失败
    LOG_ERROR("Failed to parse message with any supported format");

    return common::Result::failure(
        common::protocol::PARSER_DESERIALIZATION_FAILED,
        "Failed to parse message with any supported format (binary Protobuf or JSON)"
    );
}

common::Result ProtocolParser::validateMessage(
    const protocol::UnifiedMessage& message) const {

    std::vector<common::Result> validation_results;

    // 1. 检查必填字段
    if (message.message_type().empty()) {
        validation_results.emplace_back(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing required field: message_type"
        );
    } else {
        // 检查消息类型是否有效
        std::string msg_type = message.message_type();
        if (msg_type != "device_cmd" && msg_type != "device_state") {
            validation_results.emplace_back(
                common::protocol::PARSER_UNSUPPORTED_MSG_TYPE,
                "Invalid message_type: " + msg_type
            );
        }
    }

    if (!message.has_payload()) {
        validation_results.emplace_back(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing required field: payload"
        );
    }

    // 2. 验证负载内容
    if (message.has_payload()) {
        const auto& payload = message.payload();

        if (payload.action_type().empty()) {
            validation_results.emplace_back(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "Missing required field: payload.action_type"
            );
        }

        // 根据消息类型验证负载内容
        std::string msg_type = message.message_type();
        if (msg_type == "device_cmd") {
            if (!payload.has_device_cmd()) {
                validation_results.emplace_back(
                    common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                    "device_cmd message missing device_cmd in payload"
                );
            }
        } else if (msg_type == "device_state") {
            if (!payload.has_device_state()) {
                validation_results.emplace_back(
                    common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                    "device_state message missing device_state in payload"
                );
            }
        }
    }

    // 3. 验证时间戳格式（如果存在）
    if (!message.timestamp().empty()) {
        // 简单的时间戳格式验证：ISO 8601 或 Unix 时间戳
        std::string timestamp = message.timestamp();

        // 检查是否是数字（Unix时间戳）
        bool is_numeric = !timestamp.empty() &&
                         std::all_of(timestamp.begin(), timestamp.end(),
                                    [](char c) { return std::isdigit(c); });

        if (!is_numeric) {
            // 尝试匹配 ISO 8601 格式
            std::regex iso8601_regex(
                R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:?\d{2})?$)"
            );

            if (!std::regex_match(timestamp, iso8601_regex)) {
                validation_results.emplace_back(
                    common::protocol::VALIDATOR_INVALID_TIMESTAMP,
                    "Invalid timestamp format: " + timestamp
                );
            }
        }
    }

    // 4. 验证设备ID格式（如果存在设备状态）
    if (message.has_payload() && message.payload().has_device_state()) {
        const auto& device_state = message.payload().device_state();
        if (device_state.device_i_d().empty()) {
            validation_results.emplace_back(
                common::protocol::VALIDATOR_INVALID_DEVICE_ID,
                "Missing device ID in device state"
            );
        }
    }

    // 5. 合并所有验证结果
    if (!validation_results.empty()) {
        if (validation_results.size() == 1) {
            return validation_results[0];
        }

        // 多个验证错误，返回第一个错误并附加说明
        common::Result merged_error = validation_results[0];
        std::string detail = "Multiple validation errors (";
        detail += std::to_string(validation_results.size());
        detail += " total)";

        merged_error.setError(
            merged_error.getErrorCode(),
            merged_error.getErrorMessage(),
            detail
        );

        return merged_error;
    }

    LOG_DEBUG("Message validation passed for type: {}", message.message_type());
    return common::Result::success("Message validation passed");
}

common::Result ProtocolParser::handleDeviceCommand(
    const protocol::UnifiedMessage& message,
    Callback response_callback) {

    // 确保我们有设备命令
    if (!message.has_payload() || !message.payload().has_device_cmd()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing device command in payload"
        );
    }

    const auto& device_cmd = message.payload().device_cmd();
    std::string command_name = device_cmd.cmd();

    LOG_INFO("Handling device command: {} with action_type: {}",
             command_name, message.payload().action_type());

    // 验证命令参数
    if (command_name.empty()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing command name in device_cmd"
        );
    }

    // 根据命令类型进行特定验证
    common::Result validation_result;

    if (command_name == "new_job") {
        if (!device_cmd.has_new_job()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "new_job command missing new_job data"
            );
        }
    } else if (command_name == "new_local_job") {
        if (!device_cmd.has_new_local_job()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "new_local_job command missing new_local_job data"
            );
        }
    } else if (command_name == "light_control") {
        if (!device_cmd.has_light_control()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "light_control command missing light_control data"
            );
        }
    } else if (command_name == "temperature_control") {
        if (!device_cmd.has_temperature_control()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "temperature_control command missing temperature_control data"
            );
        }
    } else if (command_name == "stream_control") {
        if (!device_cmd.has_stream_control()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "stream_control command missing stream_control data"
            );
        }
    } else if (command_name == "user_profile") {
        if (!device_cmd.has_user_profile()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "user_profile command missing user_profile data"
            );
        }
    } else if (command_name == "device_unregister") {
        if (!device_cmd.has_device_unregister()) {
            validation_result = common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "device_unregister command missing device_unregister data"
            );
        }
    } else {
        validation_result = common::Result::failure(
            common::protocol::DISPATCHER_INVALID_ACTION,
            "Unsupported command: " + command_name
        );
    }

    if (!validation_result.isSuccess()) {
        LOG_WARN("Command validation failed: {}", validation_result.toString());

        // 如果有回调，发送错误响应
        if (response_callback) {
            response_callback(validation_result);
        }

        return validation_result;
    }

    // 创建命令上下文
    auto context = std::make_shared<swan::command::CommandContext>(
        "",  // 设备ID，可以从消息中提取或由上层设置
        message.request_id(),
        std::chrono::system_clock::now()
    );

    // 设置响应回调
    if (response_callback) {
        context->setResponseCallback(response_callback);
    }

    LOG_DEBUG("Successfully parsed and validated device command: {}", command_name);

    // 返回成功结果，包含命令和上下文
    common::Result result = common::Result::success(
        "Device command parsed and validated"
    );

    // 注意：这里不能直接存储 device_cmd 引用，因为它是局部变量
    // 在实际应用中，可能需要创建副本或使用智能指针
    // 这里我们返回一个包含命令类型的结果，实际处理由上层进行

    return result.setData(command_name);
}

common::Result ProtocolParser::handleDeviceState(
    const protocol::UnifiedMessage& message,
    Callback response_callback) {

    // 设备端通常不应该收到 device_state 消息
    // 这里记录并返回警告，但继续处理

    LOG_WARN("Device received device_state message, which is typically an uplink message");

    if (!message.has_payload() || !message.payload().has_device_state()) {
        return common::Result::failure(
            common::protocol::PARSER_MISSING_REQUIRED_FIELD,
            "Missing device state in payload"
        );
    }

    const auto& device_state = message.payload().device_state();

    LOG_DEBUG("Parsed device state from device: {}, status: {}",
             device_state.device_i_d(), device_state.status());

    // 验证设备状态数据
    if (device_state.device_i_d().empty()) {
        LOG_WARN("Device state missing device ID");
    }

    if (device_state.status().empty()) {
        LOG_WARN("Device state missing status");
    }

    // 在实际应用中，这里可能会触发某些处理，比如：
    // 1. 更新本地设备状态缓存
    // 2. 触发状态同步事件
    // 3. 记录到日志系统

    // 如果有回调，可以发送确认响应
    if (response_callback) {
        response_callback(common::Result::success("Device state received"));
    }

    return common::Result::success("Device state processed");
}

void ProtocolParser::recordMessage(bool valid, bool validation_error,
                                  uint64_t parse_time_ns) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    statistics_.total_messages++;

    if (valid) {
        statistics_.valid_messages++;
    } else if (validation_error) {
        statistics_.validation_errors++;
    } else {
        statistics_.parse_errors++;
    }

    statistics_.total_parse_time_ns += parse_time_ns;

    // 定期打印统计信息（每100条消息）
    if (statistics_.total_messages % 100 == 0) {
        LOG_DEBUG("Parser statistics - Total: {}, Valid: {}, Parse errors: {}, "
                 "Validation errors: {}, Avg parse time: {:.3f}ms",
                 statistics_.total_messages,
                 statistics_.valid_messages,
                 statistics_.parse_errors,
                 statistics_.validation_errors,
                 statistics_.getAverageParseTimeMs());
    }
}
} // namespace protocol
} // namespace swan