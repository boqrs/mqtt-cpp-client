//
// Created by wave on 2026/1/21.
//

#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include "command/cmd.h"
#include "protocol.pb.h"

namespace swan {
namespace command {

class CommandDispatcher {
public:
    // 注册处理器
    void registerHandler(const std::string& actionType,
                        std::shared_ptr<ICommandHandler> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[actionType] = std::move(handler);
    }

    // 注销处理器
    void unregisterHandler(const std::string& actionType) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(actionType);
    }

    // 分发命令（主入口）
    CommandResult dispatch(
        const device::UnifiedMessage& message,
        std::shared_ptr<CommandContext> context) {

        // 1. 验证消息类型
        if (message.message_type() != "device_cmd") {
            return CommandResult::failure(
                ErrorCode::UNSUPPORTED_MESSAGE_TYPE,
                "Unsupported message type: " + message.message_type()
            );
        }

        // 2. 提取action_type
        const std::string& actionType = message.payload().action_type();
        if (actionType.empty()) {
            return CommandResult::failure(
                ErrorCode::INVALID_ACTION_TYPE,
                "Empty action_type"
            );
        }

        // 3. 查找处理器
        std::shared_ptr<ICommandHandler> handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = handlers_.find(actionType);
            if (it == handlers_.end()) {
                return CommandResult::failure(
                    ErrorCode::UNSUPPORTED_COMMAND,
                    "Unsupported command: " + actionType
                );
            }
            handler = it->second;
        }

        // 4. 执行命令
        try {
            return handler->execute(message.payload().device_cmd(), context);
        } catch (const std::exception& e) {
            return CommandResult::failure(
                ErrorCode::INTERNAL_ERROR,
                "Command execution failed: " + actionType,
                e.what()
            );
        }
    }

    // 异步分发
    std::future<CommandResult> dispatchAsync(
        const device::UnifiedMessage& message,
        std::shared_ptr<CommandContext> context) {

        return std::async(std::launch::async, [this, &message, context]() {
            return dispatch(message, context);
        });
    }

    // 获取支持的action_type列表
    std::vector<std::string> getSupportedActions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> actions;
        actions.reserve(handlers_.size());

        for (const auto& [action, _] : handlers_) {
            actions.push_back(action);
        }

        return actions;
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ICommandHandler>> handlers_;
};

} // namespace command
} // namespace swan