//
// Created by wave on 2026/1/27.
//

#pragma once
#include "mqtt/manage.h"
#include <string>


class MqttGenericPublisher {
public:
    // 异步发布（非阻塞）
    static bool publish(const std::string& topic,
                        const std::string& payload,
                        int qos = 0,
                        bool retained = false) {
        return MqttManager::getInstance().publish(topic, payload, qos, retained);
    }

    // 同步发布（阻塞，带超时）
    static bool publishSync(const std::string& topic,
                            const std::string& payload,
                            int qos = 0,
                            bool retained = false,
                            int timeout_ms = 5000) {
        return MqttManager::getInstance().publishSync(topic, payload, qos, retained, timeout_ms);
    }
};