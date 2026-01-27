//
// Created by wave on 2026/1/27.
//

#pragma once
#include "mqtt/manage.h"
#include <string>
#include <functional>
#include <unordered_map>


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

private:
    std::unordered_map<std::string, MessageCallback> m_topic_callbacks; // Topic->回调映射
    std::mutex m_cb_mutex; // 回调映射锁
};