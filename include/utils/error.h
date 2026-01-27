//
// Created by wave on 2026/1/23.
//

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <functional>

namespace swan {
namespace common {

// ==================== 模块和子模块定义 ====================
enum class Module : uint8_t {
    SUCCESS     = 0x00,
    COMMON      = 0x01,
    PROTOCOL    = 0x02,
    COMMAND     = 0x03,
    SERVICE     = 0x04,
    NETWORK     = 0x05,
    HARDWARE    = 0x06,
    SYSTEM      = 0x0F,
};

namespace submodule {
    // 通用子模块
    enum Common : uint8_t {
        GENERIC      = 0x01,
        VALIDATION   = 0x02,
        CONFIG       = 0x03,
    };

    // 协议子模块
    enum Protocol : uint8_t {
        PARSER      = 0x01,
        SERIALIZER  = 0x02,
        VALIDATOR   = 0x03,
        DISPATCHER  = 0x04,
    };

    // 命令子模块
    enum Command : uint8_t {
        DISPATCH    = 0x01,
        EXECUTOR    = 0x02,
        CONTEXT     = 0x03,
    };

    // 服务子模块
    enum Service : uint8_t {
        LIGHT       = 0x01,
        STREAM      = 0x02,
        TEMPERATURE = 0x03,
        PRINT       = 0x04,
        MATERIAL    = 0x05,
    };

    // 硬件子模块
    enum Hardware : uint8_t {
        GPIO        = 0x01,
        UART        = 0x02,
        I2C         = 0x03,
        SPI         = 0x04,
        CAMERA      = 0x05,
        THERMAL     = 0x06,
    };
}

// ==================== 错误码类 ====================
class ErrorCode {
public:
    constexpr ErrorCode() : value_(0x00000000) {}

    constexpr ErrorCode(Module module, uint8_t submodule, uint16_t code)
        : value_((static_cast<uint32_t>(module) << 24) |
                 (static_cast<uint32_t>(submodule) << 16) |
                 (static_cast<uint32_t>(code) & 0xFFFF)) {}

    constexpr ErrorCode(uint32_t raw_value) : value_(raw_value) {}

    // 获取各部分
    constexpr Module getModule() const {
        return static_cast<Module>((value_ >> 24) & 0xFF);
    }

    constexpr uint8_t getSubmodule() const {
        return (value_ >> 16) & 0xFF;
    }

    constexpr uint16_t getCode() const {
        return value_ & 0xFFFF;
    }

    constexpr uint32_t getValue() const { return value_; }

    // 运算符重载
    constexpr bool operator==(ErrorCode other) const { return value_ == other.value_; }
    constexpr bool operator!=(ErrorCode other) const { return value_ != other.value_; }
    constexpr bool operator<(ErrorCode other) const { return value_ < other.value_; }

    // 转换为字符串
    std::string toString() const;

    // 获取错误描述
    std::string getDescription() const;

    // 检查是否成功
    constexpr bool isSuccess() const { return value_ == 0; }

    // 检查是否属于某个模块
    constexpr bool isModule(Module module) const {
        return getModule() == module;
    }

private:
    uint32_t value_;
};

// ==================== 预定义错误码 ====================

// 成功码
constexpr ErrorCode SUCCESS = ErrorCode(Module::SUCCESS, 0x00, 0x0000);

// -------------------- 通用错误 --------------------
namespace common {
    constexpr ErrorCode INVALID_PARAMETER      = ErrorCode(Module::COMMON, submodule::Common::GENERIC, 0x0001);
    constexpr ErrorCode NULL_POINTER           = ErrorCode(Module::COMMON, submodule::Common::GENERIC, 0x0002);
    constexpr ErrorCode OUT_OF_RANGE           = ErrorCode(Module::COMMON, submodule::Common::GENERIC, 0x0003);
    constexpr ErrorCode TYPE_MISMATCH          = ErrorCode(Module::COMMON, submodule::Common::GENERIC, 0x0004);
    constexpr ErrorCode VALIDATION_FAILED      = ErrorCode(Module::COMMON, submodule::Common::VALIDATION, 0x0001);
    constexpr ErrorCode MISSING_FIELD          = ErrorCode(Module::COMMON, submodule::Common::VALIDATION, 0x0002);
    constexpr ErrorCode FORMAT_ERROR           = ErrorCode(Module::COMMON, submodule::Common::VALIDATION, 0x0003);
}

// -------------------- 协议模块错误 --------------------
namespace protocol {
    constexpr ErrorCode PARSER_INVALID_PROTOCOL      = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0001);
    constexpr ErrorCode PARSER_UNSUPPORTED_MSG_TYPE  = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0002);
    constexpr ErrorCode PARSER_MISSING_REQUIRED_FIELD= ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0003);
    constexpr ErrorCode PARSER_INVALID_FIELD_FORMAT  = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0004);
    constexpr ErrorCode PARSER_MESSAGE_TOO_LARGE     = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0005);
    constexpr ErrorCode PARSER_MESSAGE_TOO_SMALL     = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0006);
    constexpr ErrorCode PARSER_CHECKSUM_MISMATCH     = ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0007);
    constexpr ErrorCode PARSER_DESERIALIZATION_FAILED= ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0008);
    constexpr ErrorCode PARSER_PARSER_INIT_FAILED= ErrorCode(Module::PROTOCOL, submodule::Protocol::PARSER, 0x0009);
    constexpr ErrorCode VALIDATOR_INVALID_TIMESTAMP  = ErrorCode(Module::PROTOCOL, submodule::Protocol::VALIDATOR, 0x0001);
    constexpr ErrorCode VALIDATOR_INVALID_DEVICE_ID  = ErrorCode(Module::PROTOCOL, submodule::Protocol::VALIDATOR, 0x0002);
    constexpr ErrorCode VALIDATOR_INVALID_REQUEST_ID = ErrorCode(Module::PROTOCOL, submodule::Protocol::VALIDATOR, 0x0003);
    constexpr ErrorCode VALIDATOR_VALUE_OUT_OF_RANGE = ErrorCode(Module::PROTOCOL, submodule::Protocol::VALIDATOR, 0x0004);
    constexpr ErrorCode DISPATCHER_INVALID_ACTION    = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0001);
    constexpr ErrorCode DISPATCHER_NO_HANDLER        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0002);
    constexpr ErrorCode DISPATCHER_INIT_FAILED        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0003);
    constexpr ErrorCode DISPATCHER_REGISTER_FAILED        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0004);
    constexpr ErrorCode DISPATCHER_QUEUE_FULL        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0005);
    constexpr ErrorCode DISPATCHER_REJECTED        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0006);
    constexpr ErrorCode DISPATCHER_INVALID_STRATEGY        = ErrorCode(Module::PROTOCOL, submodule::Protocol::DISPATCHER, 0x0007);
}

// -------------------- 命令模块错误 --------------------
namespace command {
    constexpr ErrorCode DISPATCH_UNSUPPORTED_CMD   = ErrorCode(Module::COMMAND, submodule::Command::DISPATCH, 0x0001);
    constexpr ErrorCode DISPATCH_QUEUE_FULL        = ErrorCode(Module::COMMAND, submodule::Command::DISPATCH, 0x0002);
    constexpr ErrorCode DISPATCH_TIMEOUT           = ErrorCode(Module::COMMAND, submodule::Command::DISPATCH, 0x0003);
    constexpr ErrorCode DISPATCH_CONCURRENT_LIMIT  = ErrorCode(Module::COMMAND, submodule::Command::DISPATCH, 0x0004);
    constexpr ErrorCode EXECUTOR_INVALID_PARAMS    = ErrorCode(Module::COMMAND, submodule::Command::EXECUTOR, 0x0001);
    constexpr ErrorCode EXECUTOR_FAILED            = ErrorCode(Module::COMMAND, submodule::Command::EXECUTOR, 0x0002);
    constexpr ErrorCode EXECUTOR_RESOURCE_BUSY     = ErrorCode(Module::COMMAND, submodule::Command::EXECUTOR, 0x0003);
    constexpr ErrorCode EXECUTOR_PERMISSION_DENIED = ErrorCode(Module::COMMAND, submodule::Command::EXECUTOR, 0x0004);
    constexpr ErrorCode EXECUTOR_CANCELLED         = ErrorCode(Module::COMMAND, submodule::Command::EXECUTOR, 0x0005);
}

// -------------------- 服务模块错误 --------------------
namespace service {
    constexpr ErrorCode LIGHT_HARDWARE_FAILED        = ErrorCode(Module::SERVICE, submodule::Service::LIGHT, 0x0001);
    constexpr ErrorCode LIGHT_ALREADY_ON             = ErrorCode(Module::SERVICE, submodule::Service::LIGHT, 0x0002);
    constexpr ErrorCode LIGHT_ALREADY_OFF            = ErrorCode(Module::SERVICE, submodule::Service::LIGHT, 0x0003);
    constexpr ErrorCode LIGHT_INVALID_BRIGHTNESS     = ErrorCode(Module::SERVICE, submodule::Service::LIGHT, 0x0004);
    constexpr ErrorCode LIGHT_TIMEOUT                = ErrorCode(Module::SERVICE, submodule::Service::LIGHT, 0x0005);
    constexpr ErrorCode STREAM_CAMERA_FAILED         = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0001);
    constexpr ErrorCode STREAM_ENCODER_FAILED        = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0002);
    constexpr ErrorCode STREAM_NETWORK_FAILED        = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0003);
    constexpr ErrorCode STREAM_ALREADY_RUNNING       = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0004);
    constexpr ErrorCode STREAM_NOT_RUNNING           = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0005);
    constexpr ErrorCode STREAM_SESSION_NOT_FOUND     = ErrorCode(Module::SERVICE, submodule::Service::STREAM, 0x0006);
    constexpr ErrorCode TEMP_SENSOR_FAILED           = ErrorCode(Module::SERVICE, submodule::Service::TEMPERATURE, 0x0001);
    constexpr ErrorCode TEMP_HEATER_FAILED           = ErrorCode(Module::SERVICE, submodule::Service::TEMPERATURE, 0x0002);
    constexpr ErrorCode TEMP_OUT_OF_RANGE            = ErrorCode(Module::SERVICE, submodule::Service::TEMPERATURE, 0x0003);
    constexpr ErrorCode TEMP_STABILIZATION_FAILED    = ErrorCode(Module::SERVICE, submodule::Service::TEMPERATURE, 0x0004);
    constexpr ErrorCode PRINT_FILE_NOT_FOUND         = ErrorCode(Module::SERVICE, submodule::Service::PRINT, 0x0001);
    constexpr ErrorCode PRINT_INVALID_GCODE          = ErrorCode(Module::SERVICE, submodule::Service::PRINT, 0x0002);
    constexpr ErrorCode PRINT_DEVICE_BUSY            = ErrorCode(Module::SERVICE, submodule::Service::PRINT, 0x0003);
    constexpr ErrorCode PRINT_JOB_NOT_FOUND          = ErrorCode(Module::SERVICE, submodule::Service::PRINT, 0x0004);
    constexpr ErrorCode PRINT_MATERIAL_UNAVAILABLE   = ErrorCode(Module::SERVICE, submodule::Service::PRINT, 0x0005);
    constexpr ErrorCode MATERIAL_SLOT_EMPTY          = ErrorCode(Module::SERVICE, submodule::Service::MATERIAL, 0x0001);
    constexpr ErrorCode MATERIAL_TYPE_MISMATCH       = ErrorCode(Module::SERVICE, submodule::Service::MATERIAL, 0x0002);
    constexpr ErrorCode MATERIAL_JAMMED              = ErrorCode(Module::SERVICE, submodule::Service::MATERIAL, 0x0003);
    constexpr ErrorCode MATERIAL_TEMP_TOO_LOW        = ErrorCode(Module::SERVICE, submodule::Service::MATERIAL, 0x0004);
    constexpr ErrorCode SERVICE_INIT_FAILED        = ErrorCode(Module::SERVICE, submodule::Service::MATERIAL, 0x0005);
}

// -------------------- 系统模块错误 --------------------
namespace system {
    constexpr ErrorCode INTERNAL_ERROR           = ErrorCode(Module::SYSTEM, 0x01, 0x0001);
    constexpr ErrorCode OUT_OF_MEMORY            = ErrorCode(Module::SYSTEM, 0x01, 0x0002);
    constexpr ErrorCode FILE_NOT_FOUND           = ErrorCode(Module::SYSTEM, 0x01, 0x0003);
    constexpr ErrorCode IO_ERROR                 = ErrorCode(Module::SYSTEM, 0x01, 0x0004);
    constexpr ErrorCode THREAD_ERROR             = ErrorCode(Module::SYSTEM, 0x01, 0x0005);
    constexpr ErrorCode TIMEOUT                  = ErrorCode(Module::SYSTEM, 0x01, 0x0006);
    constexpr ErrorCode RESOURCE_UNAVAILABLE     = ErrorCode(Module::SYSTEM, 0x01, 0x0007);
    constexpr ErrorCode CONFIG_ERROR             = ErrorCode(Module::SYSTEM, 0x01, 0x0008);
    constexpr ErrorCode INITIALIZATION_FAILED    = ErrorCode(Module::SYSTEM, 0x01, 0x0009);
    constexpr ErrorCode SHUTDOWN_FAILED          = ErrorCode(Module::SYSTEM, 0x01, 0x000A);
}

// ==================== 工具函数实现 ====================

// 错误码到字符串的转换实现
inline std::string ErrorCode::toString() const {
    if (isSuccess()) return "SUCCESS";

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "0x%08X", value_);
    return std::string(buffer);
}

// 获取模块名称
inline std::string moduleToString(Module module) {
    switch (module) {
        case Module::SUCCESS:    return "SUCCESS";
        case Module::COMMON:     return "COMMON";
        case Module::PROTOCOL:   return "PROTOCOL";
        case Module::COMMAND:    return "COMMAND";
        case Module::SERVICE:    return "SERVICE";
        case Module::NETWORK:    return "NETWORK";
        case Module::HARDWARE:   return "HARDWARE";
        case Module::SYSTEM:     return "SYSTEM";
        default:                 return "UNKNOWN_MODULE";
    }
}

// 获取子模块名称
inline std::string submoduleToString(Module module, uint8_t submodule) {
    switch (module) {
        case Module::COMMON:
            switch (submodule) {
                case submodule::Common::GENERIC:    return "GENERIC";
                case submodule::Common::VALIDATION: return "VALIDATION";
                case submodule::Common::CONFIG:     return "CONFIG";
                default: return "UNKNOWN_SUBMODULE";
            }
        case Module::PROTOCOL:
            switch (submodule) {
                case submodule::Protocol::PARSER:     return "PARSER";
                case submodule::Protocol::SERIALIZER: return "SERIALIZER";
                case submodule::Protocol::VALIDATOR:  return "VALIDATOR";
                case submodule::Protocol::DISPATCHER: return "DISPATCHER";
                default: return "UNKNOWN_SUBMODULE";
            }
        case Module::COMMAND:
            switch (submodule) {
                case submodule::Command::DISPATCH:  return "DISPATCH";
                case submodule::Command::EXECUTOR:  return "EXECUTOR";
                case submodule::Command::CONTEXT:   return "CONTEXT";
                default: return "UNKNOWN_SUBMODULE";
            }
        case Module::SERVICE:
            switch (submodule) {
                case submodule::Service::LIGHT:       return "LIGHT";
                case submodule::Service::STREAM:      return "STREAM";
                case submodule::Service::TEMPERATURE: return "TEMPERATURE";
                case submodule::Service::PRINT:       return "PRINT";
                case submodule::Service::MATERIAL:    return "MATERIAL";
                default: return "UNKNOWN_SUBMODULE";
            }
        default:
            return "UNKNOWN_SUBMODULE";
    }
}

// 获取完整错误描述
inline std::string ErrorCode::getDescription() const {
    if (isSuccess()) return "Success";

    static const std::unordered_map<uint32_t, std::string> descriptions = {
        // 通用错误
        {common::INVALID_PARAMETER.getValue(),     "Invalid parameter"},
        {common::NULL_POINTER.getValue(),          "Null pointer"},
        {common::OUT_OF_RANGE.getValue(),          "Value out of range"},
        {common::VALIDATION_FAILED.getValue(),     "Validation failed"},
        {common::MISSING_FIELD.getValue(),         "Missing required field"},

        // 协议错误
        {protocol::PARSER_INVALID_PROTOCOL.getValue(),       "Invalid protocol format"},
        {protocol::PARSER_UNSUPPORTED_MSG_TYPE.getValue(),   "Unsupported message type"},
        {protocol::PARSER_MISSING_REQUIRED_FIELD.getValue(), "Missing required field in message"},
        {protocol::PARSER_INVALID_FIELD_FORMAT.getValue(),   "Invalid field format"},
        {protocol::PARSER_MESSAGE_TOO_LARGE.getValue(),      "Message too large"},
        {protocol::PARSER_MESSAGE_TOO_SMALL.getValue(),      "Message too small"},
        {protocol::PARSER_CHECKSUM_MISMATCH.getValue(),      "Checksum mismatch"},
        {protocol::PARSER_DESERIALIZATION_FAILED.getValue(), "Failed to deserialize message"},
        {protocol::VALIDATOR_INVALID_TIMESTAMP.getValue(),   "Invalid timestamp"},
        {protocol::DISPATCHER_INVALID_ACTION.getValue(),     "Invalid action type"},
        {protocol::DISPATCHER_NO_HANDLER.getValue(),         "No handler for action"},

        // 命令错误
        {command::DISPATCH_UNSUPPORTED_CMD.getValue(),      "Unsupported command"},
        {command::DISPATCH_QUEUE_FULL.getValue(),           "Command queue is full"},
        {command::DISPATCH_TIMEOUT.getValue(),              "Command dispatch timeout"},
        {command::EXECUTOR_INVALID_PARAMS.getValue(),       "Invalid command parameters"},
        {command::EXECUTOR_FAILED.getValue(),               "Command execution failed"},
        {command::EXECUTOR_RESOURCE_BUSY.getValue(),        "Resource busy"},

        // 服务错误
        {service::LIGHT_HARDWARE_FAILED.getValue(),         "Light hardware operation failed"},
        {service::LIGHT_ALREADY_ON.getValue(),              "Light is already on"},
        {service::LIGHT_ALREADY_OFF.getValue(),             "Light is already off"},
        {service::STREAM_ALREADY_RUNNING.getValue(),        "Stream is already running"},
        {service::STREAM_NOT_RUNNING.getValue(),            "Stream is not running"},
        {service::STREAM_SESSION_NOT_FOUND.getValue(),      "Stream session not found"},
        {service::PRINT_FILE_NOT_FOUND.getValue(),          "Print file not found"},
        {service::PRINT_DEVICE_BUSY.getValue(),             "Print device is busy"},
        {service::PRINT_JOB_NOT_FOUND.getValue(),           "Print job not found"},
        {service::MATERIAL_SLOT_EMPTY.getValue(),           "Material slot is empty"},

        // 系统错误
        {system::INTERNAL_ERROR.getValue(),                 "Internal system error"},
        {system::OUT_OF_MEMORY.getValue(),                  "Out of memory"},
        {system::FILE_NOT_FOUND.getValue(),                 "File not found"},
        {system::IO_ERROR.getValue(),                       "Input/output error"},
        {system::TIMEOUT.getValue(),                        "Operation timeout"},
        {system::RESOURCE_UNAVAILABLE.getValue(),           "Resource unavailable"},
        {system::INITIALIZATION_FAILED.getValue(),          "Initialization failed"},
    };

    auto it = descriptions.find(value_);
    if (it != descriptions.end()) {
        return it->second;
    }

    return moduleToString(getModule()) + "." +
           submoduleToString(getModule(), getSubmodule()) + "." +
           std::to_string(getCode());
}

// 检查错误码是否属于某个范围
inline bool isProtocolError(ErrorCode code) {
    return code.isModule(Module::PROTOCOL);
}

inline bool isCommandError(ErrorCode code) {
    return code.isModule(Module::COMMAND);
}

inline bool isServiceError(ErrorCode code) {
    return code.isModule(Module::SERVICE);
}

inline bool isSystemError(ErrorCode code) {
    return code.isModule(Module::SYSTEM);
}

// 错误码分类
enum class ErrorSeverity {
    INFO,
    WARNING,
    ERROR,
    FATAL,
};

// 获取错误严重性
inline ErrorSeverity getErrorSeverity(ErrorCode code) {
    if (code.isSuccess()) return ErrorSeverity::INFO;

    Module module = code.getModule();

    if (module == Module::SYSTEM) {
        return ErrorSeverity::FATAL;
    }

    if (module == Module::HARDWARE) {
        return ErrorSeverity::ERROR;
    }

    return ErrorSeverity::ERROR;
}

// 错误信息类
class ErrorInfo {
public:
    ErrorInfo() : code_(SUCCESS) {}

    ErrorInfo(ErrorCode code, const std::string& message = "",
              const std::string& detail = "")
        : code_(code), message_(message), detail_(detail) {}

    ErrorCode getCode() const { return code_; }
    const std::string& getMessage() const { return message_; }
    const std::string& getDetail() const { return detail_; }

    void setCode(ErrorCode code) { code_ = code; }
    void setMessage(const std::string& message) { message_ = message; }
    void setDetail(const std::string& detail) { detail_ = detail; }

    bool isSuccess() const { return code_.isSuccess(); }

    std::string toString() const {
        if (isSuccess()) {
            return "Success";
        }

        std::string result = "Error " + code_.toString() + ": " + code_.getDescription();

        if (!message_.empty()) {
            result += " - " + message_;
        }

        if (!detail_.empty()) {
            result += " (" + detail_ + ")";
        }

        return result;
    }

private:
    ErrorCode code_;
    std::string message_;
    std::string detail_;
};

} // namespace common
} // namespace swan