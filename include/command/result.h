//
// Created by wave on 2026/1/21.
//
#pragma once
#include <string>
#include <memory>
#include <variant>
#include <optional>
#include <vector>

namespace swan {
namespace command {

// 错误码枚举
enum class ErrorCode : uint32_t {
    SUCCESS = 0,

    // 协议错误 (1000-1999)
    INVALID_PROTOCOL = 1000,      // 协议格式错误
    UNSUPPORTED_MESSAGE_TYPE = 1001, // 不支持的message_type
    MISSING_REQUIRED_FIELD = 1002, // 缺少必需字段
    INVALID_ACTION_TYPE = 1003,   // 无效的action_type

    // 命令执行错误 (2000-2999)
    UNSUPPORTED_COMMAND = 2000,   // 不支持的指令
    INVALID_PARAMETER = 2001,     // 参数无效
    EXECUTION_FAILED = 2002,      // 执行失败
    DEVICE_BUSY = 2003,           // 设备繁忙
    DEVICE_OFFLINE = 2004,        // 设备离线
    PERMISSION_DENIED = 2005,     // 权限不足

    // 业务逻辑错误 (3000-3999)
    JOB_NOT_FOUND = 3000,         // 作业不存在
    MATERIAL_UNAVAILABLE = 3001,  // 材料不可用
    TEMPERATURE_ERROR = 3002,     // 温度错误

    // 系统错误 (9000-9999)
    INTERNAL_ERROR = 9000,        // 内部错误
    TIMEOUT = 9001,               // 超时
    RESOURCE_UNAVAILABLE = 9002,  // 资源不可用
    NETWORK_ERROR = 9003,         // 网络错误
};

// 错误信息结构
struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::string detail;           // 详细错误信息（可选）
    std::string suggestion;       // 修复建议（可选）

    // 转换为字符串
    std::string toString() const {
        return "Error " + std::to_string(static_cast<uint32_t>(code))
               + ": " + message + (detail.empty() ? "" : " (" + detail + ")");
    }

    // 检查是否成功
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
};

// 命令执行结果
class CommandResult {
public:
    // 成功构造
    static CommandResult success(const std::string& message = "") {
        return CommandResult(ErrorCode::SUCCESS, message);
    }

    // 失败构造
    static CommandResult failure(ErrorCode code, const std::string& message,
                                 const std::string& detail = "") {
        return CommandResult(code, message, detail);
    }

    // 默认构造函数（成功）
    CommandResult() : errorInfo_{ErrorCode::SUCCESS, ""} {}

    // 带错误码的构造函数
    CommandResult(ErrorCode code, const std::string& message,
                  const std::string& detail = "", const std::string& suggestion = "")
        : errorInfo_{code, message, detail, suggestion} {}

    // 设置成功
    void setSuccess(const std::string& message = "") {
        errorInfo_ = {ErrorCode::SUCCESS, message};
    }

    // 设置错误
    void setError(ErrorCode code, const std::string& message,
                  const std::string& detail = "", const std::string& suggestion = "") {
        errorInfo_ = {code, message, detail, suggestion};
    }

    // 获取错误信息
    const ErrorInfo& getErrorInfo() const { return errorInfo_; }

    // 检查是否成功
    bool isSuccess() const { return errorInfo_.isSuccess(); }

    // 获取错误码
    ErrorCode getErrorCode() const { return errorInfo_.code; }

    // 获取错误消息
    std::string getErrorMessage() const { return errorInfo_.message; }

    // 获取详细错误信息
    std::string getErrorDetail() const { return errorInfo_.detail; }

    // 转换为字符串
    std::string toString() const {
        if (isSuccess()) {
            return "Success" + (errorInfo_.message.empty() ? "" : ": " + errorInfo_.message);
        }
        return errorInfo_.toString();
    }

    // 携带额外数据（使用variant支持多种类型）
    template<typename T>
    void setData(const T& data) {
        data_ = data;
    }

    template<typename T>
    std::optional<T> getData() const {
        if (auto* value = std::get_if<T>(&data_)) {
            return *value;
        }
        return std::nullopt;
    }

    // 执行时间统计
    void setExecutionTime(uint64_t microseconds) {
        executionTimeMicros_ = microseconds;
    }

    uint64_t getExecutionTime() const { return executionTimeMicros_; }

    // 添加子结果（用于复杂命令）
    void addSubResult(const CommandResult& subResult) {
        subResults_.push_back(subResult);
    }

    const std::vector<CommandResult>& getSubResults() const {
        return subResults_;
    }

private:
    ErrorInfo errorInfo_;
    uint64_t executionTimeMicros_ = 0;
    std::variant<std::monostate, int, double, std::string, std::vector<uint8_t>> data_;
    std::vector<CommandResult> subResults_;
};

} // namespace command
} // namespace swan