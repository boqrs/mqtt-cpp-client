
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
#include "mqtt/publisher.h"
#include "service/service_factory.h"
#include "protocol/initialize.h"
#include "config.h"
#include "service/registry.h"


using namespace std::chrono_literals;

// 全局标志，用于控制程序运行
std::atomic<bool> g_running{true};

// 信号处理函数
void signal_handler(int signal)
{
    if (signal == SIGINT) {
        LOG_INFO("Received interrupt signal, stopping...");
        g_running = false;
    }
    shutdownLogger();
}

// 自定义监听器实现
class StatusListener : public swan::mqtt::IDeviceStatusListener {
public:

    void onPublishError(const std::string& topic,
                        const std::string& error) override {
        LOG_ERROR("[Listener] Publish error: {} - {}", topic, error );
    }

    void onPublishSuccess(const std::string& topic,
                          size_t payload_size) override {
        LOG_INFO("[Listener] Published to {}, {} bytes", topic, payload_size );
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
    logCfg.asyncQueueSize = 8192;
    logCfg.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v"; // 自定义格式
    initializeLogger(logCfg);

   auto success = services::registerAllServices();
    if (!success) {
        LOG_ERROR("service initialization failed");
        shutdownLogger();
        return -1;
    }

    auto init_result = swan::init::initCommandProcessing(true, 2000);
    if (!init_result) {
        LOG_ERROR("Command processing initialization failed");
        shutdownLogger();
        return -1;
    }

    LOG_INFO("IOT device command processing started successfully");


    LOG_INFO("Press Ctrl+C to stop the program.");
    LOG_INFO("==============================");

    // 配置发布器
    mqtt::DeviceStatusPublisherConfig config;
    config.broker_ip = "10.33.44.3";
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
        shutdownLogger();
        return 1;
    }

    if (!publisher->start()) {
        LOG_ERROR("Failed to start publisher" );
        shutdownLogger();
        return 1;
    }

    // 模拟设备状态数据
    swan::protocol::DeviceStateData status;
    status.device_id = "printer_001";
    status.printer_name = "SWAN-X1";
    status.sn = DEVICE_SN;
    status.mac = "00:11:22:33:44:55";
    status.ip_address = "192.168.1.100";
    status.firmware_version = "1.0.0";
    status.status = "printing";

    // 初始化温度
    status.chamber_temp = 25;
    status.chamber_target_temp = 60;
    status.left_temperature = 200;
    status.left_target_temperature = 210;
    status.right_temperature = 205;
    status.right_target_temperature = 215;
    status.platform_cur_temperature = 60;
    status.platform_target_temperature = 65;

    // 初始化喷嘴温度
    status.nozzle_temps = {200, 205};
    status.nozzle_target_temps = {210, 215};

    // 初始化打印进度
    status.progress = 0;
    status.print_layer = 1;
    status.target_layer = 100;
    status.estimate_time = 3600;
    status.actual_duration = 0;
    status.duration = 3600;
    status.cumulative_print_time = 100;
    status.file_name = "test_model.gcode";
    status.file_path = "/prints/test_model.gcode";

    // 初始化材料信息
    status.left_filament = 500;
    status.right_filament = 450;
    status.left_filament_type = "PLA";
    status.right_filament_type = "PLA";
    status.cumulative_filament = 1500.5;

    // 其他字段
    status.camera = 1;
    status.chamber_fan = 50;
    status.cooling_fan = 30;
    status.current_speed = 100;
    status.entirety_speed = 100;
    status.estimate_length_left = 500;
    status.estimate_length_right = 450;
    status.delay_close = "false";
    status.delay_time = 0;
    status.door = "closed";
    status.external = "connected";
    status.internal = "ready";
    status.job_id = "job_001";
    status.language = "zh-CN";
    status.lidar = 1;
    status.light = "on";
    status.location = "workshop_1";
    status.measure = "normal";
    status.model_weight = 100;
    status.nozzle_count = 2;
    status.nozzle_model = "0.4mm";
    status.nozzle_style = 1;
    status.pid = "printer_001";
    status.polar_register_code = "polar_123";
    status.flash_register_code = "flash_456";
    status.remain_memory = 256.5;
    status.stream = "rtsp://192.168.1.100:8554/live";
    status.hls_stream = "http://192.168.1.100:8080/hls/live.m3u8";
    status.thumbnail_path = "/thumbnails/test_model.jpg";
    status.tvoc = 50;
    status.z_axis_compensation = 0;
    status.filling_amount = 100;

    int counter = 0;
    try {
        while (g_running) {
            // 更新状态数据
            status.progress = (status.progress + 1) % 101;
            status.actual_duration += 1;
            status.print_layer = (status.print_layer % 100) + 1;

            // 温度模拟变化
            status.left_temperature = 200 + (counter % 10);
            status.right_temperature = 205 + (counter % 10);
            status.chamber_temp = 25 + (counter % 5);

            // 材料消耗模拟
            if (counter % 10 == 0) {
                if (status.left_filament > 0) {
                    status.left_filament -= 1;
                }
                if (status.right_filament > 0) {
                    status.right_filament -= 1;
                }
                status.cumulative_filament += 0.1;
            }
            auto msg = swan::protocol::MessageFactory::createStateMessage(status);

            // 发布状态
            publisher->publish(msg, false);

            // 打印当前进度
            if (counter % 5 == 0) {
                LOG_INFO("message index: {}, process {}%, temperature: L: {} °C R: {} °C",
                         counter, status.progress, status.left_temperature, status.right_temperature);
            }

            counter++;

            // 等待1秒
            std::this_thread::sleep_for(500ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An exception occurred: {}", e.what());
    }

    // 停止发布器
    LOG_WARN("application is stopping...");

    publisher->stop();
    LOG_INFO("The program is exiting...");
    shutdownLogger();
    return 0;
}