//
// Created by wave on 2026/1/21.
//
// CommandContext.h
#pragma once
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include "command/result.h"

namespace swan {
    namespace command {

        class CommandContext {
        public:
            CommandContext(const std::string& deviceId,
                           const std::string& requestId,
                           const std::chrono::system_clock::time_point& timestamp)
                : deviceId_(deviceId)
                , requestId_(requestId)
                , timestamp_(timestamp) {}

            const std::string& getDeviceId() const { return deviceId_; }

            const std::string& getRequestId() const { return requestId_; }

            std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }

            void setResponseCallback(std::function<void(const std::string&, const CommandResult&)> callback) {
                responseCallback_ = std::move(callback);
            }

            void sendResponse(const CommandResult& result) const {
                if (responseCallback_) {
                    responseCallback_(requestId_, result);
                }
            }

        private:
            std::string deviceId_;
            std::string requestId_;
            std::chrono::system_clock::time_point timestamp_;
            std::function<void(const std::string&, const CommandResult&)> responseCallback_;
        };

    } // namespace command
} // namespace swan