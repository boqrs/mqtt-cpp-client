//
// Created by wave on 2026/1/21.
//

// ICommandHandler.h
#pragma once
#include <memory>
#include <string>
#include <future>
#include "protocol.pb.h"
#include "command/context.h"
#include "command/result.h"


namespace swan {
    namespace command {

        class ICommandHandler {
        public:
            virtual ~ICommandHandler() = default;

            virtual std::string getSupportedActionType() const = 0;

            virtual CommandResult execute(
                const device::ControlCommand& cmd,
                const std::shared_ptr<CommandContext>& context) = 0;

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