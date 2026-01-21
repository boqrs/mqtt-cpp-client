
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
    /*void onStatusChanged(const swan::protocol::DeviceStateData& new_status,
                         const swan::protocol::DeviceStateData& old_status,
                         const std::vector<std::string>& changed_fields) override {
        std::cout << "[Listener] Status changed: ";
        for (const auto& field : changed_fields) {
            std::cout << field << " ";
        }
        std::cout << std::endl;
    }*/

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
    config.broker_ip = "10.33.44.104";
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
    swan::protocol::DeviceStateData status;
    status.device_id = "printer_001";
    status.printer_name = "SWAN-X1";
    status.sn = "SN2024001";
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
        LOG_ERROR("发生异常: {}", e.what());
    }

    // 停止发布器
    LOG_WARN("正在停止发布器...");

    publisher->stop();
    LOG_INFO("总计发布{} 条消息", counter);
    LOG_INFO("程序退出");
    return 0;
}