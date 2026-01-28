//
// Created by wave on 2026/1/21.
//

#pragma once

#include <mutex>
#include "protocol.pb.h"
#include "utils/result.h"

namespace swan {
namespace proparser {

// 协议版本
struct ProtocolVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;

    std::string toString() const {
        return std::to_string(major) + "." +
               std::to_string(minor) + "." +
               std::to_string(patch);
    }

    bool operator==(const ProtocolVersion& other) const {
        return major == other.major &&
               minor == other.minor &&
               patch == other.patch;
    }
};

// 协议解析器
class ProtocolParser {
public:
    using Callback = std::function<void(const common::Result&)>;

    ProtocolParser();
    ~ProtocolParser();

    // 主解析接口
    common::Result parseAndHandle(
        std::string_view raw_data,
        Callback response_callback = nullptr);

    // 协议配置
    void setProtocolVersion(const ProtocolVersion& version);
    ProtocolVersion getProtocolVersion() const;

    void setStrictMode(bool strict) { strict_mode_ = strict; }
    bool getStrictMode() const { return strict_mode_; }

    // 统计信息
    struct Statistics {
        uint64_t total_messages = 0;
        uint64_t valid_messages = 0;
        uint64_t parse_errors = 0;
        uint64_t validation_errors = 0;
        uint64_t total_parse_time_ns = 0;

        void reset() {
            total_messages = 0;
            valid_messages = 0;
            parse_errors = 0;
            validation_errors = 0;
            total_parse_time_ns = 0;
        }

        double getAverageParseTimeMs() const {
            if (total_messages == 0) return 0.0;
            return static_cast<double>(total_parse_time_ns) / total_messages / 1'000'000.0;
        }
    };

    Statistics getStatistics() const;
    void resetStatistics();

private:
    // 解析步骤
    common::Result parseMessage(
        std::string_view raw_data,
        protocol::UnifiedMessage& message);

    common::Result validateMessage(
        const protocol::UnifiedMessage& message) const;

    // 消息处理
    common::Result handleDeviceCommand(
        const protocol::UnifiedMessage& message,
        Callback response_callback);

    common::Result handleDeviceState(
        const protocol::UnifiedMessage& message,
        Callback response_callback);

    // 统计更新
    void recordMessage(bool valid, bool validation_error, uint64_t parse_time_ns);

    ProtocolVersion protocol_version_;
    bool strict_mode_ = true;

    // 统计数据和保护锁
    mutable std::mutex stats_mutex_;
    Statistics statistics_;
};

} // namespace protocol
} // namespace swan