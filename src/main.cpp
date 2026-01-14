
//
// Created by wave on 2026/1/7.
//

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <chrono>

#include "spdlog/spdlog.h"
#include "logger/logger.h"
#include "mqtt/device_status_publisher.h"


using namespace std::chrono_literals;

// 全局标志，用于控制程序运行
std::atomic<bool> g_running{true};

// 信号处理函数
void signal_handler(int signal)
{
    if (signal == SIGINT) {
        std::cout << "\n收到中断信号，正在停止..." << std::endl;
        g_running = false;
    }
}

// 自定义监听器实现
class StatusListener : public swan::mqtt::IDeviceStatusListener {
public:
    void onStatusChanged(const swan::models::DeviceStatusData& new_status,
                         const swan::models::DeviceStatusData& old_status,
                         const std::vector<std::string>& changed_fields) override {
        std::cout << "[Listener] Status changed: ";
        for (const auto& field : changed_fields) {
            std::cout << field << " ";
        }
        std::cout << std::endl;
    }

    void onPublishError(const std::string& topic,
                        const std::string& error) override {
        std::cerr << "[Listener] Publish error: " << topic
                  << " - " << error << std::endl;
    }

    void onPublishSuccess(const std::string& topic,
                          size_t payload_size) override {
        std::cout << "[Listener] Published to " << topic
                  << " (" << payload_size << " bytes)" << std::endl;
    }
};

int main() {
    using namespace swan;
    // 注册信号处理
    std::signal(SIGINT, signal_handler);

    // 1. 初始化日志系统
    LoggerConfig logCfg;
    logCfg.level = LogLevel::DEBUG_LEVEL;  // 设置日志级别为DEBUG，这样DEBUG及以上的日志都会输出
    logCfg.consoleOutput = true;           // 输出到控制台
    logCfg.fileOutput = false;             // 不输出到文件
    logCfg.asyncLogging = true;            // 使用异步日志（如果你的实现支持）
    logCfg.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v"; // 自定义格式
    initializeLogger(logCfg);

    LOG_INFO("按 Ctrl+C 停止程序");
    LOG_INFO("==============================");

    // 配置发布器
    mqtt::DeviceStatusPublisherConfig config;
    config.broker_ip = "10.33.44.240";
    config.broker_port = 18082;
    config.client_id = "swan_printer_001";
    config.username = "admin";
    config.password = "zxcv.1234";
    config.mode = mqtt::DeviceStatusPublisherConfig::PublishMode::HYBRID;
    config.publish_interval_ms = 10000;  // 10秒心跳
    config.qos = 1;

    // 创建发布器
    auto publisher = std::make_unique<mqtt::DeviceStatusPublisher>();

    // 添加监听器
    auto listener = std::make_shared<StatusListener>();
    publisher->addListener(listener);

    // 初始化并启动
    if (!publisher->initialize(config)) {
        LOG_ERROR("Failed to initialize publisher" );
        return 1;
    }

    if (!publisher->start()) {
        LOG_ERROR("Failed to start publisher" );
        return 1;
    }

    // 模拟设备状态数据
    models::DeviceStatus status;
    auto& data = status.getMutableData();
    data.device.device_id = "printer_001";
    data.device.printer_name = "SWAN-X1";
    data.device.sn = "SN2024001";
    data.device.mac = "00:11:22:33:44:55";
    data.device.ip_address = "192.168.1.100";
    data.device.firmware_version = "1.0.0";
    data.device.status = "printing";

    // 初始化温度
    data.temperatures.chamber_temp = 25;
    data.temperatures.chamber_target_temp = 60;
    data.temperatures.left_temperature = 200;
    data.temperatures.left_target_temperature = 210;
    data.temperatures.right_temperature = 205;
    data.temperatures.right_target_temperature = 215;
    data.temperatures.platform_cur_temperature = 60;
    data.temperatures.platform_target_temperature = 65;

    // 初始化喷嘴温度
    data.temperatures.nozzle_temps = {200, 205};
    data.temperatures.nozzle_target_temps = {210, 215};

    // 初始化打印进度
    data.progress.progress = 0;
    data.progress.print_layer = 1;
    data.progress.target_layer = 100;
    data.progress.estimate_time = 3600;
    data.progress.actual_duration = 0;
    data.progress.duration = 3600;
    data.progress.cumulative_print_time = 100;
    data.progress.file_name = "test_model.gcode";
    data.progress.file_path = "/prints/test_model.gcode";

    // 初始化材料信息
    data.materials.left_filament = 500;
    data.materials.right_filament = 450;
    data.materials.left_filament_type = "PLA";
    data.materials.right_filament_type = "PLA";
    data.materials.cumulative_filament = 1500.5;

    // 其他字段
    data.camera = 1;
    data.chamber_fan = 50;
    data.cooling_fan = 30;
    data.current_speed = 100;
    data.entirety_speed = 100;
    data.estimate_length_left = 500;
    data.estimate_length_right = 450;
    data.delay_close = "false";
    data.delay_time = 0;
    data.door = "closed";
    data.external = "connected";
    data.internal = "ready";
    data.job_id = "job_001";
    data.language = "zh-CN";
    data.lidar = 1;
    data.light = "on";
    data.location = "workshop_1";
    data.measure = "normal";
    data.model_weight = 100;
    data.nozzle_count = 2;
    data.nozzle_model = "0.4mm";
    data.nozzle_style = 1;
    data.pid = "printer_001";
    data.polar_register_code = "polar_123";
    data.flash_register_code = "flash_456";
    data.remain_memory = 256.5;
    data.stream = "rtsp://192.168.1.100:8554/live";
    data.hls_stream = "http://192.168.1.100:8080/hls/live.m3u8";
    data.thumbnail_path = "/thumbnails/test_model.jpg";
    data.tvoc = 50;
    data.z_axis_compensation = 0;
    data.filling_amount = 100;

    // 设置事件类型
    status.setEventType("printing");
    status.setActionType("status_report");

    int counter = 0;
    try {
        while (g_running) {
            // 更新状态数据
            data.progress.progress = (data.progress.progress + 1) % 101;
            data.progress.actual_duration += 1;
            data.progress.print_layer = (data.progress.print_layer % 100) + 1;

            // 温度模拟变化
            data.temperatures.left_temperature = 200 + (counter % 10);
            data.temperatures.right_temperature = 205 + (counter % 10);
            data.temperatures.chamber_temp = 25 + (counter % 5);

            // 材料消耗模拟
            if (counter % 10 == 0) {
                if (data.materials.left_filament > 0) {
                    data.materials.left_filament -= 1;
                }
                if (data.materials.right_filament > 0) {
                    data.materials.right_filament -= 1;
                }
                data.materials.cumulative_filament += 0.1;
            }

            // 发布状态
            publisher->updateStatus(status);

            // 每10次发布一个特殊事件
            if (counter % 10 == 0) {
                if (counter % 20 == 0) {
                    publisher->publishEvent("layer_completed");
                } else {
                    publisher->publishEvent("progress_update");
                }
            }

            // 打印当前进度
            if (counter % 5 == 0) {
                LOG_INFO("发布第{}条消息, 进度 {}%, 温度: L: {} °C R: {} °C",
                         counter, data.progress.progress, data.temperatures.left_temperature, data.temperatures.right_temperature);
            }

            counter++;

            // 等待1秒
            std::this_thread::sleep_for(500ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("发生异常: {}", e.what());
    }

    // 停止发布器
    LOG_WARN("正在停止发布器...");

    publisher->stop();
    LOG_INFO("总计发布{} 条消息", counter);
    LOG_INFO("程序退出");
    return 0;
}