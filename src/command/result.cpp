//
// Created by wave on 2026/1/21.
//

#pragma once
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "protocol.pb.h"
#include "command/result.h"

namespace swan {
namespace protocol {

class ResponseBuilder {
public:
    // 构建成功的响应
    static device::UnifiedMessage buildSuccessResponse(
        const std::string& requestId,
        const std::string& actionType,
        const std::string& message = "") {

        device::UnifiedMessage response;
        response.set_message_type("command_response");
        response.set_request_id(requestId);
        response.set_timestamp(getCurrentTimestamp());

        auto* payload = response.mutable_payload();
        payload->set_action_type(actionType + "_response");
      //  payload->set_result_code(0); // 0表示成功
       // payload->set_result_message(message.empty() ? "OK" : message);

        return response;
    }

    // 构建错误响应
    static device::UnifiedMessage buildErrorResponse(
        const std::string& requestId,
        const std::string& actionType,
        const command::ErrorInfo& errorInfo) {

        device::UnifiedMessage response;
        response.set_message_type("command_response");
        response.set_request_id(requestId);
        response.set_timestamp(getCurrentTimestamp());

        auto* payload = response.mutable_payload();
        payload->set_action_type(actionType + "_response");
       // payload->set_result_code(static_cast<uint32_t>(errorInfo.code));
       // payload->set_result_message(errorInfo.message);

        if (!errorInfo.detail.empty()) {
         //   payload->set_result_detail(errorInfo.detail);
        }

        return response;
    }

    // 从CommandResult构建响应
    static device::UnifiedMessage buildFromCommandResult(
        const std::string& requestId,
        const std::string& actionType,
        const command::CommandResult& cmdResult) {

        if (cmdResult.isSuccess()) {
            return buildSuccessResponse(requestId, actionType, cmdResult.getErrorMessage());
        } else {
            return buildErrorResponse(requestId, actionType, cmdResult.getErrorInfo());
        }
    }

private:
    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

} // namespace protocol
} // namespace swan