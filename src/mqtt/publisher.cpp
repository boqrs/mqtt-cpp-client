//
// Created by wave on 2026/1/12.
//
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <zlib.h>
#include "logger/logger.h"
#include "mqtt/publisher.h"
#include "config.h"

//TODO: 这里有问题，状态的发布数据分为两种情况
//      1. 一种是需要立刻发送的同步消息比较急，可能是将来的告警信息 action_type=alarm
//      2. 一种是常规的状态更新，会被放到消息缓冲链表里面，由 mqtt 线程按顺序发布
//      3. 需要按照上面两需求修改发布逻辑, 并且由发布标记决定是否立刻发布
//      4. 到发布器的时候不应该再去解析协议中的内容决定怎么走
namespace swan {
    namespace mqtt {

        namespace {
            // 辅助函数：压缩数据
            std::string compressData(const std::string& data) {
                if (data.empty()) return data;

                uLongf dest_len = compressBound(data.size());
                std::vector<Bytef> compressed(dest_len);

                if (compress(compressed.data(), &dest_len,
                             reinterpret_cast<const Bytef*>(data.data()),
                             data.size()) != Z_OK) {
                    return data;  // 压缩失败，返回原始数据
                }

                compressed.resize(dest_len);
                return std::string(reinterpret_cast<char*>(compressed.data()), dest_len);
            }

            // 辅助函数：生成时间戳字符串
            std::string getTimestampString() {
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now.time_since_epoch()) % 1000;

                std::stringstream ss;
                ss << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S")
                   << "_" << std::setfill('0') << std::setw(3) << now_ms.count();
                return ss.str();
            }
        }

        class DeviceStatusPublisher::Impl {
        public:
            Impl() : last_publish_time_(std::chrono::steady_clock::now()) {}

            bool initialize(const DeviceStatusPublisherConfig& config) {
                config_ = config;

                // 创建MQTT线程
                mqtt_thread_ = std::make_unique<MqttThread>(
                        config.broker_ip,
                        config.broker_port,
                        config.client_id.empty() ?
                        "swan_device_" + getTimestampString() : config.client_id
                );

                //设置认证信息
                mqtt_thread_->setAuth(config_.username, config_.password);
                // 设置消息回调
                mqtt_thread_->setMessageCallback([this](const std::string& topic,
                                                        const std::string& payload) {
                    // 可以处理发布确认等
                    handleMqttMessage(topic, payload);
                });

                // 设置状态回调
                mqtt_thread_->setStatusCallback([this](const std::string& status,
                                                       bool connected) {
                    handleMqttStatus(status, connected);
                });

                return true;
            }

            bool start() {
                if (!mqtt_thread_) return false;

                if (!mqtt_thread_->start(true, 5)) {  // 启用自动重连，5秒间隔
                    return false;
                }

                is_running_ = true;

                // 如果启用周期性发布，启动定时器线程
                /*if (config_.mode == DeviceStatusPublisherConfig::PublishMode::PERIODIC ||
                    config_.mode == DeviceStatusPublisherConfig::PublishMode::HYBRID) {
                    timer_thread_ = std::thread([this]() { timerLoop(); });
                }*/

                return true;
            }

            void stop() {
                is_running_ = false;

                /*if (timer_thread_.joinable()) {
                    timer_thread_.join();
                }*/

                if (mqtt_thread_) {
                    mqtt_thread_->stop();
                }
            }

            void pulish(const protocol::UnifiedMessage& msg, bool force_publish) {
                std::lock_guard<std::mutex> lock(status_mutex_);

                // 保存旧状态用于比较
                protocol::DeviceStateData old_status = current_status_;

                if (!msg.isDeviceStateMessage()) {
                    LOG_ERROR("only support state to publish");
                    return;
                }

                current_status_ = *msg.getDeviceStateData();//TODO: 这里是不是有问题～～～～～～

                // 检查哪些字段发生了变化
                auto changed_fields = detectChanges(old_status, current_status_);

                // 通知监听器
                for (auto& listener : listeners_) {
                    if (listener) {
                        /*listener->onStatusChanged(status,
                                                  old_status,
                                                  changed_fields);*/
                    }
                }

                // 根据发布模式决定是否发布
                bool should_publish = false;
                bool is_sync = false;//后续根据消息级别修改是否同步发送

                switch (config_.mode) {
                    case DeviceStatusPublisherConfig::PublishMode::EVENT_BASED:
                        should_publish = !changed_fields.empty() || force_publish;
                        break;

                    case DeviceStatusPublisherConfig::PublishMode::PERIODIC:
                        // 周期性发布，后续再支持
                        //should_publish = true;
                        break;

                    case DeviceStatusPublisherConfig::PublishMode::HYBRID:
                        should_publish = (!changed_fields.empty() &&
                                          meetsThreshold(changed_fields, old_status, current_status_)) ||
                                         force_publish;
                        break;
                }

                if (should_publish) {
                    publishMsg(msg, is_sync);
                }
            }

        private:
            // 检测状态变化
            std::vector<std::string> detectChanges(const protocol::DeviceStateData& old_status,
                                                   const protocol::DeviceStateData& new_status) {
                std::vector<std::string> changes;

                // 简化的变化检测，实际应该比较所有重要字段
                const auto& old_data = old_status;
                const auto& new_data = new_status;

                if (old_data.chamber_temp != new_data.chamber_temp) {
                    changes.push_back("chamber_temp");
                }

                if (old_data.progress != new_data.progress) {
                    changes.push_back("progress");
                }

                if (old_data.left_filament != new_data.left_filament ||
                    old_data.right_filament != new_data.right_filament) {
                    changes.push_back("filament");
                }

                // 可以添加更多字段的比较

                return changes;
            }

            // 检查是否达到发布阈值
            bool meetsThreshold(const std::vector<std::string>& changed_fields,
                                const protocol::DeviceStateData& old_status,
                                const protocol::DeviceStateData& new_status) {
                const auto& old_data = old_status;
                const auto& new_data = new_status;

                for (const auto& field : changed_fields) {
                    if (field == "chamber_temp") {
                        float diff = std::abs(static_cast<float>(
                                                      new_data.chamber_temp -
                                                      old_data.chamber_temp));
                        if (diff >= config_.filters.temperature_threshold) {
                            return true;
                        }
                    } else if (field == "progress") {
                        int diff = std::abs(static_cast<int>(
                                                    new_data.progress - old_data.progress));
                        if (diff >= config_.filters.progress_threshold) {
                            return true;
                        }
                    }
                    // 可以添加其他字段的阈值检查
                }

                return false;
            }

            // 发布状态到MQTT
            bool publishMsg(const protocol::UnifiedMessage& msg,
                               bool is_sync) {
                if (!mqtt_thread_ || !mqtt_thread_->isConnected()) {
                    return false;
                }

                // 序列化为Protobuf
                std::string serialized_data;
                try {
                    serialized_data = msg.toString();
                } catch (const std::exception& e) {
                    std::cerr << "[Publisher] Failed to serialize status: "
                              << e.what() << std::endl;
                    return false;
                }

                // 如果需要，进行压缩
                if (config_.enable_compression &&
                    serialized_data.size() >= config_.compression_threshold) {
                    std::string compressed = compressData(serialized_data);
                    if (compressed.size() < serialized_data.size()) {
                        serialized_data = std::move(compressed);
                        // 可以在消息中添加压缩标记
                    }
                }

                // 生成主题
                std::string topic = generateTopic(config_);
                if (topic.empty()) {
                    LOG_ERROR("failed to generate Topic, sn is empty ");
                    return false;
                }

                bool success = false;
                if (!is_sync) {
                    mqtt_thread_->publish(topic, serialized_data, config_.qos, 0);
                }else {
                    success = mqtt_thread_->publishSync(
        topic, serialized_data, config_.qos, 2000);
                }

                // 更新统计信息
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.total_published++;
                    stats_.total_bytes += serialized_data.size();
                    if (!success) {
                        stats_.failed_publishes++;
                    }
                    auto now = std::chrono::steady_clock::now();
                    if (stats_.last_publish_time.time_since_epoch().count() > 0) {
                        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - stats_.last_publish_time).count();
                        stats_.average_publish_interval_ms =
                                (stats_.average_publish_interval_ms * (stats_.total_published - 1) +
                                 interval) / stats_.total_published;
                    }
                    stats_.last_publish_time = now;
                }

                // 通知监听器
                for (auto& listener : listeners_) {
                    if (listener) {
                        if (success) {
                            listener->onPublishSuccess(topic, serialized_data.size());
                        } else {
                            listener->onPublishError(topic, "Publish failed");
                        }
                    }
                }

                return success;
            }

            void handleMqttMessage(const std::string& topic,
                                   const std::string& payload) {
                // 处理MQTT消息，如发布确认等
            }

            void handleMqttStatus(const std::string& status,
                                  bool connected) {
                // 处理MQTT连接状态变化
            }

            // 成员变量
            DeviceStatusPublisherConfig config_;
            std::unique_ptr<MqttThread> mqtt_thread_;
            protocol::DeviceStateData current_status_;
            std::vector<std::shared_ptr<IDeviceStatusListener>> listeners_;
            Statistics stats_;

            std::mutex status_mutex_;
            std::mutex stats_mutex_;
            std::mutex listener_mutex_;

           // std::thread timer_thread_;
            std::atomic<bool> is_running_{false};
            std::chrono::steady_clock::time_point last_publish_time_;
        };

// DeviceStatusPublisher 实现
        DeviceStatusPublisher::DeviceStatusPublisher()
                : pimpl_(std::make_unique<Impl>()) {}

        DeviceStatusPublisher::~DeviceStatusPublisher() {
            stop();
        }

        bool DeviceStatusPublisher::initialize(const DeviceStatusPublisherConfig& config) {
            return pimpl_->initialize(config);
        }

        bool DeviceStatusPublisher::start() {
            return pimpl_->start();
        }

        void DeviceStatusPublisher::stop() {
            pimpl_->stop();
        }

        void DeviceStatusPublisher::publish(const protocol::UnifiedMessage& msg,
                                                 bool force_publish) {
            pimpl_->pulish(msg, force_publish);
        }

        void DeviceStatusPublisher::setConfig(const DeviceStatusPublisherConfig& config) {
            // 需要重新初始化
        }

        const DeviceStatusPublisherConfig& DeviceStatusPublisher::getConfig() const {
            static DeviceStatusPublisherConfig empty_config;
            return empty_config;  // 实际应该返回pimpl中的config
        }

        void DeviceStatusPublisher::addListener(
                std::shared_ptr<IDeviceStatusListener> listener) {
            // 实现监听器添加
        }

        void DeviceStatusPublisher::removeListener(
                std::shared_ptr<IDeviceStatusListener> listener) {
            // 实现监听器移除
        }

        const DeviceStatusPublisher::Statistics& DeviceStatusPublisher::getStatistics() const {
            static Statistics empty_stats;
            return empty_stats;  // 实际应该返回pimpl中的stats
        }

        void DeviceStatusPublisher::resetStatistics() {
            // 实现统计重置
        }

        std::string DeviceStatusPublisher::generateTopic(
                const DeviceStatusPublisherConfig& config) {
            std::stringstream topic;
            topic << config.base_topic;

            topic << "/" << DEVICE_MODE<<"/"<<DEVICE_SN;

            return topic.str();
        }

    } // namespace mqtt
} // namespace swan