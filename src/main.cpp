
//
// Created by wave on 2026/1/7.
//
//TODO: 这里有问题，状态的发布数据分为两种情况
//      1. 一种是需要立刻发送的同步消息比较急，可能是将来的告警信息 action_type=alarm
//      2. 一种是常规的状态更新，会被放到消息缓冲链表里面，由 mqtt 线程按顺序发布
//      3. 需要按照上面两需求修改发布逻辑, 并且由发布标记决定是否立刻发布
//      4. 到发布器的时候不应该再去解析协议中的内容决定怎么走

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <chrono>

#include "spdlog/spdlog.h"
#include "logger/logger.h"
#include "../include/mqtt/initializer.h"
#include "../include/mqtt/manage.h"
#include "../include/service/base/service_factory.h"
#include "protocol/initialize.h"
#include "config.h"
#include "../include/service/base/registry.h"
#include "protocol.pb.h"

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

int main() {
    using namespace swan;
    using namespace swan::device; // 引入 Proto 生成的命名空间

    // 注册信号处理
    std::signal(SIGINT, signal_handler);

    // 1. 初始化日志系统
    LoggerConfig logCfg;
    logCfg.level = LogLevel::DEBUG_LEVEL;  // 设置日志级别为DEBUG
    logCfg.consoleOutput = true;           // 输出到控制台
    logCfg.fileOutput = false;             // 不输出到文件
    logCfg.asyncLogging = true;            // 使用异步日志
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

    if (!initMqtt()) {
        LOG_ERROR("failed to init mqtt client, progress exit...");
        shutdownLogger();
        return -1;
    }

    // 模拟设备状态数据（使用 Protobuf setter 方法赋值）
    protocol::DeviceStateData status;
    // 修正字段名：device_id → device_i_d，使用 set_xxx() 方法
    status.set_device_i_d("printer_001");
    status.set_printer_name("SWAN-X1");
    status.set_sn(DEVICE_SN);
    status.set_mac("00:11:22:33:44:55");
    status.set_ip_address("192.168.1.100");
    status.set_firmware_version("1.0.0");
    status.set_status("printing");

    // 初始化温度（setter 方法）
    status.set_chamber_temp(25);
    status.set_chamber_target_temp(60);
    status.set_left_temperature(200);
    status.set_left_target_temperature(210);
    status.set_right_temperature(205);
    status.set_right_target_temperature(215);
    status.set_platform_cur_temperature(60);
    status.set_platform_target_temperature(65);

    // 初始化喷嘴温度（repeated 字段用 Add() 方法）
    status.add_nozzle_temps(200);
    status.add_nozzle_temps(205);
    status.add_nozzle_target_temps(210);
    status.add_nozzle_target_temps(215);

    // 初始化打印进度（setter 方法）
    status.set_progress(0);
    status.set_print_layer(1);
    status.set_target_layer(100);
    status.set_estimate_time(3600);
    status.set_actual_duration(0);
    status.set_duration(3600);
    status.set_cumulative_print_time(100);
    status.set_file_name("test_model.gcode");
    status.set_file_path("/prints/test_model.gcode");

    // 初始化材料信息（setter 方法）
    status.set_left_filament(500);
    status.set_right_filament(450);
    status.set_left_filament_type("PLA");
    status.set_right_filament_type("PLA");
    status.set_cumulative_filament(1500.5);

    // 其他字段（setter 方法，修正 job_id → job_i_d）
    status.set_camera(1);
    status.set_chamber_fan(50);
    status.set_cooling_fan(30);
    status.set_current_speed(100);
    status.set_entirety_speed(100);
    status.set_estimate_length_left(500);
    status.set_estimate_length_right(450);
    status.set_delay_close("false");
    status.set_delay_time(0);
    status.set_door("closed");
    status.set_external("connected");
    status.set_internal("ready");
    status.set_job_i_d("job_001"); // 修正字段名
    status.set_language("zh-CN");
    status.set_lidar(1);
    status.set_light("on");
    status.set_location("workshop_1");
    status.set_measure("normal");
    status.set_model_weight(100);
    status.set_nozzle_count(2);
    status.set_nozzle_model("0.4mm");
    status.set_nozzle_style(1);
    status.set_pid("printer_001");
    status.set_polar_register_code("polar_123");
    status.set_flash_register_code("flash_456");
    status.set_remain_memory(256.5);
    status.set_stream("rtsp://192.168.1.100:8554/live");
    status.set_hls_stream("http://192.168.1.100:8080/hls/live.m3u8");
    status.set_thumbnail_path("/thumbnails/test_model.jpg");
    status.set_tvoc(50);
    status.set_z_axis_compensation(0);
    status.set_filling_amount(100);

    int counter = 0;
    try {
        while (g_running) {
            // 更新状态数据（使用 setter 方法）
            status.set_progress((status.progress() + 1) % 101);
            status.set_actual_duration(status.actual_duration() + 1);
            status.set_print_layer((status.print_layer() % 100) + 1);

            // 温度模拟变化
            status.set_left_temperature(200 + (counter % 10));
            status.set_right_temperature(205 + (counter % 10));
            status.set_chamber_temp(25 + (counter % 5));

            // 材料消耗模拟
            if (counter % 10 == 0) {
                if (status.left_filament() > 0) {
                    status.set_left_filament(status.left_filament() - 1);
                }
                if (status.right_filament() > 0) {
                    status.set_right_filament(status.right_filament() - 1);
                }
                status.set_cumulative_filament(status.cumulative_filament() + 0.1);
            }

            // 手动构建 UnifiedMessage（替代不存在的 MessageFactory）
            protocol::UnifiedMessage msg;
            // 设置消息类型（上行设备状态）
            msg.set_message_type("device_state");
            msg.set_name("device_state_report");
            msg.set_timestamp(std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            msg.set_request_id("req_" + std::to_string(counter));

            // 构建 Payload 并关联 DeviceStateData
            protocol::Payload* payload = msg.mutable_payload();
            payload->set_action_type("device_state_report");
            // 将 DeviceStateData 赋值到 Payload 的 device_state 字段
            payload->mutable_device_state()->CopyFrom(status);

            // Protobuf 二进制序列化（核心：MQTT 传输二进制数据）
            std::string binary_payload;
            if (!msg.SerializeToString(&binary_payload)) {
                LOG_ERROR("Protobuf二进制序列化失败");
                shutdownLogger();
                return -1; // 修正：返回错误码，而非空 return
            }

            // 发布 MQTT 消息：使用二进制序列化结果，而非 toString()
            bool publish_ok = MqttManager::getInstance().publish(
                MQTT_DEVICE_STATIC_TOPIC,
                binary_payload // 发布二进制数据（Protobuf 原生格式）
            );
            if (!publish_ok) {
                LOG_WARN("MQTT 消息发布失败，index: {}", counter);
            }

            // 打印当前进度
            if (counter % 5 == 0) {
                LOG_INFO("message index: {}, process {}%, temperature: L: {} °C R: {} °C",
                         counter, status.progress(), status.left_temperature(), status.right_temperature());
            }

            counter++;
            // 等待500ms
            std::this_thread::sleep_for(500ms);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("An exception occurred: {}", e.what());
        shutdownLogger();
        return -1; // 修正：异常时返回错误码
    }

    // 停止发布器
    LOG_INFO("The program is exiting...");
    shutdownLogger();
    return 0;
}