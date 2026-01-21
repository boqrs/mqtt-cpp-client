//
// Created by wave on 2026/1/21.
//

#pragma once
#include <functional>
#include <memory>
#include "command/dispatcher.h"
#include "command/context.h"
#include "protocol.pb.h"

namespace swan {
namespace protocol {

class ProtocolParser {
public:
    explicit ProtocolParser(command::CommandDispatcher& dispatcher)
        : dispatcher_(dispatcher) {}

    /**
     * @brief 解析并处理MQTT消息（核心入口函数）
     * @param data 原始MQTT消息数据
     * @param responseCallback 响应回调函数
     */
    void parseAndHandle(const std::vector<uint8_t>& data,
                       std::function<void(const command::CommandResult&)> responseCallback) {

        // 1. 反序列化
        device::UnifiedMessage message;
        if (!message.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            // 解析失败，直接返回错误
            command::CommandResult result = command::CommandResult::failure(
                command::ErrorCode::INVALID_PROTOCOL,
                "Failed to parse protocol buffer"
            );
            if (responseCallback) responseCallback(result);
            return;
        }

        // 2. 创建执行上下文
        auto context = std::make_shared<command::CommandContext>(
            "single-device",  // 单设备场景，固定设备ID
            message.request_id(),
            std::chrono::system_clock::now()
        );

        context->setResponseCallback([responseCallback](const command::CommandResult& result) {
            if (responseCallback) responseCallback(result);
        });

        // 3. 根据message_type处理
        if (message.message_type() == "device_cmd") {
            // 下行命令：交给dispatcher处理
            auto future = dispatcher_.dispatchAsync(message, context);

            // 异步等待结果并触发回调
            std::async(std::launch::async, [future = std::move(future), context]() mutable {
                try {
                    auto result = future.get();
                    context->sendResponse(result);
                } catch (const std::exception& e) {
                    auto errorResult = command::CommandResult::failure(
                        command::ErrorCode::INTERNAL_ERROR,
                        "Async execution failed",
                        e.what()
                    );
                    context->sendResponse(errorResult);
                }
            });

        } else if (message.message_type() == "device_state") {
            // 上行状态：这里应该是设备发送状态，但单设备场景不需要处理
            // 可以记录日志或忽略
            if (responseCallback) {
                responseCallback(command::CommandResult::success(
                    "Device state received"
                ));
            }
        } else {
            // 未知消息类型
            if (responseCallback) {
                responseCallback(command::CommandResult::failure(
                    command::ErrorCode::UNSUPPORTED_MESSAGE_TYPE,
                    "Unknown message type: " + message.message_type()
                ));
            }
        }
    }

private:
    command::CommandDispatcher& dispatcher_;
};

} // namespace protocol
} // namespace swan