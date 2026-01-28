//
// Created by wave on 2026/1/23.
//

#pragma once

#include <memory>
#include <string>
#include <future>
#include <vector>
#include "protocol/command/context.h"
#include "protocol.pb.h"
#include "utils/result.h"

namespace swan { namespace device { class ControlCommand; } }

namespace swan {
   // namespace protocol {
        namespace command {
            using ControlCommand = ::swan::protocol::ControlCommand;

            class CommandContext;

            // 执行模式
            enum class ExecutionMode {
                SYNC,
                ASYNC,
                DETACHED
            };

            // 命令优先级
            enum class CommandPriority {
                LOW = 0,
                NORMAL = 5,
                HIGH = 10,
                CRITICAL = 15
            };

            // 命令处理器接口
            class ICommandHandler {
            public:
                virtual ~ICommandHandler() = default;

                // 基础信息
                virtual std::string getName() const = 0;
                virtual std::string getVersion() const = 0;
                virtual std::string getDescription() const = 0;

                // 命令支持
                virtual std::string getSupportedActionType() const = 0;
                virtual std::vector<std::string> getSupportedCommands() const = 0;

                // 执行模式
                virtual ExecutionMode getExecutionMode() const = 0;
                virtual CommandPriority getPriority() const = 0;
                virtual uint32_t getTimeoutMs() const = 0;

                // 执行方法
                virtual common::Result execute(
                    const ControlCommand& cmd,
                    const std::shared_ptr<CommandContext>& context) = 0;

                virtual std::future<common::Result> executeAsync(
                    const ControlCommand& cmd,
                    const std::shared_ptr<CommandContext>& context) {
                    return std::async(std::launch::async, [this, &cmd, &context]() {
                        return execute(cmd, context);
                    });
                }

                // 验证命令参数
                virtual common::Result validateCommand(
                    const protocol::ControlCommand& cmd) const {
                    return common::Result::success();
                }

                // 生命周期管理
                virtual bool initialize() { return true; }
                virtual bool shutdown() { return true; }

                // 状态查询
                virtual bool isBusy() const { return false; }
                virtual std::string getStatus() const { return "READY"; }
            };

        } // namespace command
   // } // namespace protocol
} // namespace swan