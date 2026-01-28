//
// Created by wave on 2026/1/23.
//


#include <fstream>
#include <sys/stat.h>
#include "service/cloud_job_service.h"
#include "logger/logger.h"
#include "utils/result.h"
#include "service/service_factory.h"

namespace swan {
namespace services {
    bool fileExists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

PrintService::PrintService()
    : BaseService({
        .name = "PrintService",
        .version = "1.0.0",
        .description = "Manages 3D printing jobs",
        .action_type = "print_control",
        .execution_mode = command::ExecutionMode::ASYNC,
        .priority = command::CommandPriority::CRITICAL,
        .timeout_ms = 0,  // 长时间运行，无超时
        .max_concurrent = 1,  // 一次只能打印一个作业
        .require_ack = true,
        .dependencies = {"temperature_service", "material_service"}
    }) {

    // 启动打印处理线程
    print_thread_ = std::thread(&PrintService::printProcessor, this);

    LOG_DEBUG("PrintService created and processor thread started");
}

PrintService::~PrintService() {
    // 停止处理线程
    processing_ = false;
    job_cv_.notify_all();

    if (print_thread_.joinable()) {
        print_thread_.join();
    }

    LOG_DEBUG("PrintService destroyed");
}

std::vector<std::string> PrintService::getSupportedCommands() const {
    return {"new_job", "new_local_job"};
}

common::Result PrintService::validateCommand(
    const swan::protocol::ControlCommand& cmd) const {

    std::string command = cmd.cmd();

    if (command == "new_job") {
        if (!cmd.has_new_job()) {
            return common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "Missing new_job data"
            );
        }

        const auto& job_cmd = cmd.new_job();

        // 验证作业信息
        if (job_cmd.job_info_size() == 0) {
            return common::Result::failure(
                common::common::INVALID_PARAMETER,
                "No job info provided"
            );
        }

        for (const auto& job_info : job_cmd.job_info()) {
            if (job_info.sn().empty() || job_info.uuid().empty()) {
                return common::Result::failure(
                    common::common::INVALID_PARAMETER,
                    "Invalid job info (missing SN or UUID)"
                );
            }
        }

        if (!job_cmd.has_args()) {
            return common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "Missing job arguments"
            );
        }

        // 验证作业参数
        return validateJobParameters(job_cmd.args());

    } else if (command == "new_local_job") {
        if (!cmd.has_new_local_job()) {
            return common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "Missing new_local_job data"
            );
        }

        const auto& local_job_cmd = cmd.new_local_job();
        if (!local_job_cmd.has_args()) {
            return common::Result::failure(
                common::protocol::PARSER_MISSING_REQUIRED_FIELD,
                "Missing local job arguments"
            );
        }

        const auto& args = local_job_cmd.args();
        if (args.job_id().empty() || args.file_name().empty()) {
            return common::Result::failure(
                common::common::INVALID_PARAMETER,
                "Invalid local job arguments"
            );
        }

    } else {
        return common::Result::failure(
            common::command::DISPATCH_UNSUPPORTED_CMD,
            "Unsupported command for PrintService",
            "Got: " + command
        );
    }

    return common::Result::success("Command validation passed");
}

common::Result PrintService::validateJobParameters(const swan::protocol::NewJobArgs& args) const {
    if (args.filename().empty()) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Missing filename"
        );
    }

    if (args.filepath().empty()) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Missing filepath"
        );
    }

    // 验证材料信息
    for (const auto& material : args.ms()) {
        auto result = validateMaterialInfo(material);
        if (!result.isSuccess()) {
            return result;
        }
    }

    return common::Result::success();
}

common::Result PrintService::validateMaterialInfo(const protocol::MaterialInfo& material) const{
    if (material.slot() < 0) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Invalid material slot"
        );
    }

    if (material.t_num() < 0) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Invalid tool head number"
        );
    }

    if (material.material_type().empty()) {
        return common::Result::failure(
            common::common::INVALID_PARAMETER,
            "Missing material type"
        );
    }

    return common::Result::success();
}

common::Result PrintService::doExecute(
    const swan::protocol::ControlCommand& cmd,
    const std::shared_ptr<command::CommandContext>& context) {

    std::string command = cmd.cmd();

    if (command == "new_job") {
        return startNewJob(cmd.new_job(), context);
    } else if (command == "new_local_job") {
        return startNewLocalJob(cmd.new_local_job(), context);
    }

    return common::Result::failure(
        common::command::DISPATCH_UNSUPPORTED_CMD,
        "Unknown print command",
        command
    );
}

common::Result PrintService::startNewJob(const protocol::NewJobCmd& job_cmd,
                                        const std::shared_ptr<command::CommandContext>& context) {

    try {
        const auto& args = job_cmd.args();

        LOG_INFO("Starting new print job: {}", args.filename());

        // 创建作业对象
        auto job = std::make_shared<PrintJob>();
        job->job_id = job_cmd.job_info(0).uuid();  // 使用第一个作业的UUID
        job->filename = args.filename();
        job->filepath = args.filepath();
        job->leveling = args.leveling();
        job->flow_calibration = args.flow_calibration();
        job->use_ms = args.use_ms();
        job->t_count = args.t_count();
        job->start_time = std::chrono::system_clock::now();

        // 复制材料信息
        for (const auto& material : args.ms()) {
            job->materials.push_back(material);
        }

        // 验证文件是否存在
        if (!fileExists(args.filepath())) {
            return common::Result::failure(
                common::service::PRINT_FILE_NOT_FOUND,
                "Print file not found",
                args.filepath()
            );
        }

        // 将作业加入队列
        {
            std::lock_guard<std::mutex> lock(job_mutex_);
            job_queue_.push(job);
        }

        job_cv_.notify_one();

        LOG_INFO("Print job {} added to queue, total jobs in queue: {}",
                 job->job_id, job_queue_.size());

        return common::Result::success("Print job queued successfully")
            .setData(job->job_id);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start new print job: {}", e.what());
        return common::Result::failure(
            common::service::PRINT_JOB_NOT_FOUND,
            "Failed to start print job",
            e.what()
        );
    }
}

common::Result PrintService::startNewLocalJob(const protocol::NewLocalJobCmd& local_job_cmd,
                                             const std::shared_ptr<command::CommandContext>& context) {

    try {
        const auto& args = local_job_cmd.args();

        LOG_INFO("Starting new local print job: {}", args.file_name());

        // 创建作业对象
        auto job = std::make_shared<PrintJob>();
        job->job_id = args.job_id();
        job->filename = args.file_name();
        job->leveling = args.leveling();
        job->flow_calibration = args.flow_calibration();
        job->use_ms = args.use_ms();
        job->t_count = args.t_count();
        job->start_time = std::chrono::system_clock::now();

        // 复制材料信息
        for (const auto& material : args.ms()) {
            job->materials.push_back(material);
        }

        // 将作业加入队列
        {
            std::lock_guard<std::mutex> lock(job_mutex_);
            job_queue_.push(job);
        }

        job_cv_.notify_one();

        LOG_INFO("Local print job {} added to queue", job->job_id);

        return common::Result::success("Local print job queued successfully");

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start local print job: {}", e.what());
        return common::Result::failure(
            common::service::PRINT_JOB_NOT_FOUND,
            "Failed to start local print job",
            e.what()
        );
    }
}

void PrintService::printProcessor() {
    processing_ = true;

    LOG_DEBUG("Print processor thread started");

    while (processing_) {
        std::shared_ptr<PrintJob> job;

        // 等待作业
        {
            std::unique_lock<std::mutex> lock(job_mutex_);
            job_cv_.wait(lock, [this]() {
                return !job_queue_.empty() || !processing_;
            });

            if (!processing_) {
                break;
            }

            if (job_queue_.empty()) {
                continue;
            }

            job = job_queue_.front();
            job_queue_.pop();
        }

        // 设置当前作业
        current_job_ = job;

        try {
            LOG_INFO("Starting to process print job: {}", job->filename);

            // 步骤1: 加载打印文件
            auto result = loadPrintFile(job->filepath);
            if (!result.isSuccess()) {
                LOG_ERROR("Failed to load print file: {}", result.toString());
                continue;
            }

            // 步骤2: 执行调平（如果需要）
            if (job->leveling) {
                LOG_INFO("Performing bed leveling...");
                result = executeLeveling();
                if (!result.isSuccess()) {
                    LOG_ERROR("Leveling failed: {}", result.toString());
                    continue;
                }
            }

            // 步骤3: 执行流量校准（如果需要）
            if (job->flow_calibration) {
                LOG_INFO("Performing flow calibration...");
                result = executeFlowCalibration();
                if (!result.isSuccess()) {
                    LOG_ERROR("Flow calibration failed: {}", result.toString());
                    continue;
                }
            }

            // 步骤4: 设置材料（如果需要）
            if (job->use_ms) {
                LOG_INFO("Setting up materials...");
                result = setupMaterials(job->materials);
                if (!result.isSuccess()) {
                    LOG_ERROR("Material setup failed: {}", result.toString());
                    continue;
                }
            }

            // 步骤5: 开始打印
            LOG_INFO("Starting print...");
            result = startPrinting();
            if (!result.isSuccess()) {
                LOG_ERROR("Print start failed: {}", result.toString());
                continue;
            }

            // 模拟打印过程
            for (int i = 0; i <= 100 && !job->cancelled; i += 10) {
                if (job->paused) {
                    LOG_INFO("Print paused at {}%", i);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                job->progress = i;
                LOG_INFO("Print progress: {}%", i);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }

            if (job->cancelled) {
                LOG_WARN("Print job {} was cancelled", job->job_id);
            } else {
                LOG_INFO("Print job {} completed successfully", job->job_id);
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing print job {}: {}", job->job_id, e.what());
        }

        // 清理当前作业
        current_job_.reset();
    }

    LOG_DEBUG("Print processor thread stopped");
}

// 硬件接口方法（模拟实现）
common::Result PrintService::loadPrintFile(const std::string& filepath) {
    try {
        LOG_DEBUG("Loading print file: {}", filepath);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return common::Result::success("Print file loaded");
    } catch (...) {
        return common::Result::failure(
            common::service::PRINT_INVALID_GCODE,
            "Failed to load print file"
        );
    }
}

common::Result PrintService::executeLeveling() {
    try {
        LOG_DEBUG("Executing bed leveling");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return common::Result::success("Bed leveling completed");
    } catch (...) {
        return common::Result::failure(
            common::service::PRINT_DEVICE_BUSY,
            "Bed leveling failed"
        );
    }
}

common::Result PrintService::executeFlowCalibration() {
    try {
        LOG_DEBUG("Executing flow calibration");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return common::Result::success("Flow calibration completed");
    } catch (...) {
        return common::Result::failure(
            common::service::PRINT_DEVICE_BUSY,
            "Flow calibration failed"
        );
    }
}

common::Result PrintService::setupMaterials(const std::vector<protocol::MaterialInfo>& materials) {
    try {
        LOG_DEBUG("Setting up materials");

        for (const auto& material : materials) {
            LOG_DEBUG("  Slot {}: Tool {}, Material {}, Color {}->{}",
                     material.slot(), material.t_num(),
                     material.material_type(), material.s_rgb(), material.t_rgb());

            // 检查材料是否可用
            if (material.material_type() != "PLA" &&
                material.material_type() != "ABS" &&
                material.material_type() != "PETG") {
                return common::Result::failure(
                    common::service::PRINT_MATERIAL_UNAVAILABLE,
                    "Unsupported material type",
                    material.material_type()
                );
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        return common::Result::success("Materials setup completed");

    } catch (...) {
        return common::Result::failure(
            common::service::PRINT_MATERIAL_UNAVAILABLE,
            "Material setup failed"
        );
    }
}

common::Result PrintService::startPrinting() {
    try {
        LOG_DEBUG("Starting print process");
        return common::Result::success("Print started");
    } catch (...) {
        return common::Result::failure(
            common::service::PRINT_DEVICE_BUSY,
            "Failed to start printing"
        );
    }
}

} // namespace services
} // namespace swan
