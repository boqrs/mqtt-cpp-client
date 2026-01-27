//
// Created by wave on 2026/1/27.
//

#pragma once
#include "mqtt/manage.h"
#include <string>
#include <functional>
#include <unordered_map>

/**
 * @brief 通用MQTT订阅器（支持多Topic独立回调，无硬编码）
 */
class MqttGenericSubscriber {
public:
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

    MqttGenericSubscriber() {
        MqttManager::getInstance().setMessageCallback([this](const std::string& topic, const std::string& payload) {
            std::lock_guard<std::mutex> lock(m_cb_mutex);
            auto it = m_topic_callbacks.find(topic);
            if (it != m_topic_callbacks.end()) {
                it->second(topic, payload);
            }
        });
    }

    bool subscribe(const std::string& topic, int qos, MessageCallback cb) {
        std::lock_guard<std::mutex> lock(m_cb_mutex);
        m_topic_callbacks[topic] = std::move(cb);
        return MqttManager::getInstance().subscribe(topic, qos);
    }

    bool unsubscribe(const std::string& topic) {
        std::lock_guard<std::mutex> lock(m_cb_mutex);
        m_topic_callbacks.erase(topic);
        return MqttManager::getInstance().unsubscribe(topic);
    }

    // /**
    //  * @brief 设备指令订阅（示例：仅封装Payload解析，Topic由外部传入）
    //  * @param topic 指令订阅主题
    //  * @param qos QoS等级
    //  * @param cmd_cb 指令解析后的回调
    //  * @return 是否成功
    //  */
    // bool subscribeDeviceCommand(const std::string& topic,
    //                             int qos,
    //                             std::function<void(const std::string& cmd, const std::string& params)> cmd_cb) {
    //     return subscribe(topic, qos, [cmd_cb](const std::string& topic, const std::string& payload) {
    //         // 仅封装Payload解析，Topic由调用方指定
    //         size_t cmd_pos = payload.find("\"cmd\":\"") + 6;
    //         size_t cmd_end = payload.find("\"", cmd_pos);
    //         std::string cmd = (cmd_pos != std::string::npos) ? payload.substr(cmd_pos, cmd_end - cmd_pos) : "";
    //
    //         size_t params_pos = payload.find("\"params\":\"") + 9;
    //         size_t params_end = payload.find("\"", params_pos);
    //         std::string params = (params_pos != std::string::npos) ? payload.substr(params_pos, params_end - params_pos) : "";
    //
    //         if (cmd_cb) {
    //             cmd_cb(cmd, params);
    //         }
    //     });
    // }

private:
    std::unordered_map<std::string, MessageCallback> m_topic_callbacks; // Topic->回调映射
    std::mutex m_cb_mutex; // 回调映射锁
};