//
// Created by wave on 2026/1/27.
//
#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <sstream>

#include "config.h"
#include "logger/logger.h"

#ifndef MQTT_SUBSCRIBE_TOPICS

#define MQTT_SUBSCRIBE_TOPICS "device/ad5x/SNWMOD12332211222/command:1"
#endif

class MqttInitializer {
public:
    // 单例模式（全局唯一）
    static MqttInitializer& getInstance();

    // 禁止拷贝
    MqttInitializer(const MqttInitializer&) = delete;
    MqttInitializer& operator=(const MqttInitializer&) = delete;
    bool initialize();

    void shutdown();

    inline std::vector<std::pair<std::string, int>> parseMqttSubscribeTopics() {
        std::vector<std::pair<std::string, int>> topics;
        const std::string topics_str = MQTT_SUBSCRIBE_TOPICS;

        if (topics_str.empty()) {
            LOG_ERROR("MQTT_SUBSCRIBE_TOPICS is empty (check CMake config)");
            return topics;
        }

        // 按分号分割主题项
        std::stringstream ss(topics_str);
        std::string item;
        while (std::getline(ss, item, ';')) {
            item.erase(
                std::remove_if(
                    item.begin(),
                    item.end(),
                    [](unsigned char c) { return std::isspace(c) != 0; }
                ),
                item.end()
            );

            if (item.empty()) continue;

            // 分割topic和qos
            size_t colon_pos = item.find(':');
            if (colon_pos == std::string::npos) {
                LOG_ERROR("Invalid topic format: %s (expected 'topic:qos')", item.c_str());
                continue;
            }

            std::string topic = item.substr(0, colon_pos);
            std::string qos_str = item.substr(colon_pos + 1);

            // 解析QoS
            int qos = -1;
            try {
                qos = std::stoi(qos_str);
            } catch (...) {
                LOG_ERROR("Invalid QoS for topic %s: %s", topic.c_str(), qos_str.c_str());
                continue;
            }

            if (qos >= 0 && qos <= 2) {
                topics.emplace_back(topic, qos);
            } else {
                LOG_ERROR("QoS %d out of range (0-2) for topic: %s", qos, topic.c_str());
            }
        }

        return topics;
    }

private:
    MqttInitializer();
    ~MqttInitializer() = default;

    bool reconnectWithBackoff(int retry_count, int max_retry);

    void onConnectionStatusChanged(bool connected, const std::string& reason);

    bool subscribeTopics();

    std::mutex m_mutex;               // 初始化状态锁
    std::mutex m_cv_mutex;            // 条件变量锁
    std::condition_variable m_cv;     // 连接状态通知条件变量
    bool m_connected;                 // 当前连接状态
    bool m_initialized;               // 初始化完成标记
    int m_current_retry;              // 当前重连次数
    std::vector<std::pair<std::string, int>> m_subscribe_topics; // 待订阅主题列表
    bool m_use_aws_iot = false;
    std::string m_aws_iot_endpoint;
    std::string m_aws_iot_root_ca_path;
    std::string m_aws_iot_cert_path;
    std::string m_aws_iot_private_key_path;
};

bool initMqtt();
void shutdownMqtt();