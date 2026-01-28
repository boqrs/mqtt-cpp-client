//
// Created by wave on 2026/1/23.
//

#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "utils/result.h"

namespace swan {
//namespace protocol {
namespace command {

class CommandContext {
public:
    using ResponseCallback = std::function<void(const common::Result&)>;
    using PropertyMap = std::unordered_map<std::string, std::string>;

    CommandContext(const std::string& device_id,
                   const std::string& request_id,
                   std::chrono::system_clock::time_point timestamp)
        : device_id_(device_id)
        , request_id_(request_id)
        , timestamp_(timestamp)
        , start_time_(std::chrono::steady_clock::now()) {}

    // 基础信息访问
    const std::string& getDeviceId() const { return device_id_; }
    const std::string& getRequestId() const { return request_id_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }

    // 响应回调设置
    void setResponseCallback(ResponseCallback callback) {
        response_callback_ = std::move(callback);
    }

    void sendResponse(const common::Result& result) const {
        if (response_callback_) {
            response_callback_(result);
        }
    }

    // 执行时间统计
    uint64_t getElapsedMicroseconds() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            end - start_time_).count();
    }

    // 属性存储
    void setProperty(const std::string& key, const std::string& value) {
        properties_[key] = value;
    }

    std::string getProperty(const std::string& key,
                           const std::string& default_value = "") const {
        auto it = properties_.find(key);
        return it != properties_.end() ? it->second : default_value;
    }

    const PropertyMap& getAllProperties() const { return properties_; }

    // 会话支持
    void setSessionId(const std::string& session_id) { session_id_ = session_id; }
    const std::string& getSessionId() const { return session_id_; }

private:
    std::string device_id_;
    std::string request_id_;
    std::string session_id_;
    std::chrono::system_clock::time_point timestamp_;
    std::chrono::steady_clock::time_point start_time_;
    ResponseCallback response_callback_;
    PropertyMap properties_;
};

} // namespace command
//} // namespace protocol
} // namespace swan