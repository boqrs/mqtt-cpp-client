//
// Created by wave on 2026/1/23.
//

// services/print_service.h
// cloud_job_service.h 添加 const
#pragma once

#include "base/base_service.h"
#include <queue>
#include <condition_variable>

namespace swan {
namespace services {

class PrintService : public BaseService {
public:
    PrintService();
    ~PrintService() override;

    // 重写基类方法
    std::vector<std::string> getSupportedCommands() const override;
    common::Result validateCommand(const ControlCommand& cmd) const override;

protected:
    common::Result doExecute(
        const ControlCommand& cmd,
        const std::shared_ptr<command::CommandContext>& context) override;

private:
    // 打印作业状态
    struct PrintJob {
        std::string job_id;
        std::string filename;
        std::string filepath;
        std::chrono::system_clock::time_point start_time;
        std::atomic<int> progress{0};
        std::atomic<bool> cancelled{false};
        std::atomic<bool> paused{false};

        // 作业参数
        bool leveling = false;
        bool flow_calibration = false;
        bool use_ms = false;
        int32_t t_count = 0;
        std::vector<protocol::MaterialInfo> materials;
    };

    std::shared_ptr<PrintJob> current_job_;
    std::queue<std::shared_ptr<PrintJob>> job_queue_;
    mutable std::mutex job_mutex_;
    std::condition_variable job_cv_;
    std::atomic<bool> processing_{false};
    std::thread print_thread_;

    // 作业处理方法
    common::Result startNewJob(const protocol::NewJobCmd& job_cmd,
                              const std::shared_ptr<command::CommandContext>& context);
    common::Result startNewLocalJob(const protocol::NewLocalJobCmd& local_job_cmd,
                                   const std::shared_ptr<command::CommandContext>& context);

    // 验证材料信息
    common::Result validateMaterialInfo(const protocol::MaterialInfo& material) const;
    common::Result validateJobParameters(const protocol::NewJobArgs& args) const;  // 添加 const

    // 打印线程
    void printProcessor();

    // 硬件接口
    common::Result loadPrintFile(const std::string& filepath);
    common::Result executeLeveling();
    common::Result executeFlowCalibration();
    common::Result setupMaterials(const std::vector<protocol::MaterialInfo>& materials);
    common::Result startPrinting();
    common::Result pausePrinting();
    common::Result resumePrinting();
    common::Result cancelPrinting();
};

} // namespace services
} // namespace swan