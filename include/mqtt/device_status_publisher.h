
//
// Created by wave on 2026/1/12.
//
#pragma once

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <vector>
#include <unordered_map>

#include "utils/models/device_status.h"
#include "thread.h"

namespace swan {
    namespace mqtt {

/**
 * @brief 设备状态发布配置
 */
        struct DeviceStatusPublisherConfig {
            // 基础配置
            std::string base_topic = "swan/device/status";
            std::string client_id;
            std::string broker_ip;
            std::string username;
            std::string password;
            int broker_port = 1883;

            // 发布策略
            enum class PublishMode {
                EVENT_BASED,      // 事件驱动（状态变化时发布）
                PERIODIC,         // 周期性发布
                HYBRID            // 混合模式（周期性+事件驱动）
            } mode = PublishMode::HYBRID;

            int publish_interval_ms = 10000;  // 周期性发布间隔
            int qos = 1;                      // 默认QoS等级
            bool retained = false;            // 是否保留消息

            // 过滤配置
            struct FilterConfig {
                bool enable_temperature = true;
                bool enable_progress = true;
                bool enable_materials = true;
                bool enable_device_info = true;
                bool enable_other_fields = true;

                // 变化阈值（只有变化超过阈值时才发布）
                float temperature_threshold = 1.0f;     // 温度变化阈值（℃）
                int progress_threshold = 1;             // 进度变化阈值（%）
                int material_threshold_percent = 5;     // 材料变化阈值（%）
            } filters;

            // 压缩选项
            bool enable_compression = false;            // 是否启用压缩
            size_t compression_threshold = 1024;        // 压缩阈值（字节）
        };

/**
 * @brief 设备状态变化监听器接口
 */
        class IDeviceStatusListener {
        public:
            virtual ~IDeviceStatusListener() = default;

            // 状态变化回调
            virtual void onStatusChanged(const models::DeviceStatusData& new_status,
                                         const models::DeviceStatusData& old_status,
                                         const std::vector<std::string>& changed_fields) = 0;

            // 错误回调
            virtual void onPublishError(const std::string& topic,
                                        const std::string& error) = 0;

            // 发布成功回调
            virtual void onPublishSuccess(const std::string& topic,
                                          size_t payload_size) = 0;
        };

/**
 * @brief Protobuf设备状态发布器
 *
 * 负责将设备状态序列化为Protobuf格式并通过MQTT发布
 */
        class DeviceStatusPublisher {
        public:
            DeviceStatusPublisher();
            ~DeviceStatusPublisher();

            // 禁止拷贝
            DeviceStatusPublisher(const DeviceStatusPublisher&) = delete;
            DeviceStatusPublisher& operator=(const DeviceStatusPublisher&) = delete;

            /**
             * @brief 初始化发布器
             * @param config 发布配置
             * @return 是否初始化成功
             */
            bool initialize(const DeviceStatusPublisherConfig& config);

            /**
             * @brief 启动发布器
             * @return 是否启动成功
             */
            bool start();

            /**
             * @brief 停止发布器
             */
            void stop();

            /**
             * @brief 更新设备状态
             * @param status 新的设备状态
             * @param force_publish 是否强制发布（忽略变化阈值）
             */
            void updateStatus(const models::DeviceStatus& status, bool force_publish = false);

            /**
             * @brief 立即发布当前状态（忽略发布策略）
             * @param event_type 事件类型（如：status_update, alarm, heartbeat）
             * @param action_type 动作类型（如：printing, paused, completed）
             */
            bool publishImmediately(const std::string& event_type = "status_update",
                                    const std::string& action_type = "");

            /**
             * @brief 发布特定事件
             * @param event_type 事件类型
             * @param status 状态数据（可选）
             */
            bool publishEvent(const std::string& event_type,
                              const models::DeviceStatusData* status = nullptr);

            // 配置管理
            void setConfig(const DeviceStatusPublisherConfig& config);
            const DeviceStatusPublisherConfig& getConfig() const;

            // 监听器管理
            void addListener(std::shared_ptr<IDeviceStatusListener> listener);
            void removeListener(std::shared_ptr<IDeviceStatusListener> listener);

            // 统计信息
            struct Statistics {
                size_t total_published = 0;
                size_t total_bytes = 0;
                size_t failed_publishes = 0;
                std::chrono::steady_clock::time_point last_publish_time;
                double average_publish_interval_ms = 0;
            };

            const Statistics& getStatistics() const;
            void resetStatistics();

            // 工具方法
            static std::string generateTopic(const DeviceStatusPublisherConfig& config,
                                             const models::DeviceStatus& status,
                                             const std::string& event_type = "");

        private:
            // 内部实现
            class Impl;
            std::unique_ptr<Impl> pimpl_;
        };

/**
 * @brief 轻量级发布器（用于资源受限环境）
 */
        class LightweightStatusPublisher {
        public:
            LightweightStatusPublisher(std::shared_ptr<MqttThread> mqtt_thread,
                                       const std::string& base_topic);

            /**
             * @brief 直接发布设备状态
             * @param status 设备状态
             * @param qos QoS等级
             * @return 是否发布成功
             */
            bool publish(const models::DeviceStatus& status, int qos = 1);

            /**
             * @brief 发布原始Protobuf数据
             * @param protobuf_data 序列化后的Protobuf数据
             * @param topic 主题（可选，使用默认主题如果为空）
             * @param qos QoS等级
             * @return 是否发布成功
             */
            bool publishRaw(const std::string& protobuf_data,
                            const std::string& topic = "",
                            int qos = 1);

        private:
            std::shared_ptr<MqttThread> mqtt_thread_;
            std::string base_topic_;
        };

    } // namespace mqtt
} // namespace swan