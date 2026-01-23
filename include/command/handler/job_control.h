//
// Created by wave on 2026/1/21.
//

#pragma once
#include "command/cmd.h"
#include "command/result.h"
#include "service/job_service.h"
#include "protocol.pb.h"
#include <memory>
#include <vector>

#include "service/light_service.h"

namespace swan {
namespace command {

class NewJobHandler : public ICommandHandler {
public:
    explicit NewJobHandler(std::shared_ptr<service::JobService> jobService)
        : jobService_(std::move(jobService)) {}

    std::string getSupportedActionType() const override {
        return "new_job";
    }

    CommandResult execute(
        const device::ControlCommand& cmd,
        const std::shared_ptr<CommandContext>& context) override {

        // 验证命令类型
        if (!cmd.has_new_job()) {
            return CommandResult::failure(
                ErrorCode::INVALID_PARAMETER,
                "Missing new_job field"
            );
        }

        const auto& newJobCmd = cmd.new_job();

        // 验证必要的作业信息
        if (newJobCmd.job_info_size() == 0) {
            return CommandResult::failure(
                ErrorCode::INVALID_PARAMETER,
                "No job info provided"
            );
        }

        // 提取作业信息
        std::vector<business::JobInfo> jobInfos;
        for (const auto& jobInfo : newJobCmd.job_info()) {
            if (jobInfo.sn().empty() || jobInfo.uuid().empty()) {
                return CommandResult::failure(
                    ErrorCode::INVALID_PARAMETER,
                    "Invalid job info (missing SN or UUID)"
                );
            }
            jobInfos.push_back({jobInfo.sn(), jobInfo.uuid()});
        }

        // 调用作业服务
        try {
            business::JobResult jobResult = jobService_->createJobs(
                context->getDeviceId(),
                jobInfos,
                convertToJobArgs(newJobCmd.args())
            );

            if (jobResult.success) {
                CommandResult result = CommandResult::success("Jobs created successfully");
                result.setData(jobResult.jobIds); // 携带创建的作业ID
                return result;
            } else {
                return CommandResult::failure(
                    ErrorCode::EXECUTION_FAILED,
                    "Failed to create jobs",
                    jobResult.errorMessage
                );
            }

        } catch (const std::exception& e) {
            return CommandResult::failure(
                ErrorCode::INTERNAL_ERROR,
                "Job creation exception",
                e.what()
            );
        }
    }

private:
    service::JobArgs convertToJobArgs(const device::NewJobArgs& protoArgs) {
        service::JobArgs args;
        args.filename = protoArgs.filename();
        args.filepath = protoArgs.filepath();
        args.printNow = protoArgs.print_now();
        args.leveling = protoArgs.leveling();
        // ... 其他字段转换
        return args;
    }

    std::shared_ptr<service::JobService> jobService_;
};

} // namespace command
} // namespace swan