//
// Created by wave on 2026/1/21.
//

// ICommandHandler.h
#pragma once
#include <memory>
#include <string>
#include <future>
#include "protocol.pb.h"  // 你的协议头文件
#include "command/context.h"
#include "command/result.h"


namespace swan {
    namespace command {

        class ICommandHandler {
        public:
            virtual ~ICommandHandler() = default;

            // 返回处理器支持的 action_type
            virtual std::string getSupportedActionType() const = 0;

            // 执行命令
            virtual CommandResult execute(
                const device::ControlCommand& cmd,
                const std::shared_ptr<CommandContext>& context) = 0;

            // 异步执行（可选）
            virtual std::future<CommandResult> executeAsync(
                const device::ControlCommand& cmd,
                const std::shared_ptr<CommandContext>& context) {
                return std::async(std::launch::async, [this, &cmd, &context]() {
                    return execute(cmd, context);
                });
            }
        };

    } // namespace command
} // namespace swan