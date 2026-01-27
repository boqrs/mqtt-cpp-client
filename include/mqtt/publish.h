//
// Created by wave on 2026/1/27.
//

#pragma once
#include "mqtt/manage.h"
#include <string>

/**
 * @brief 通用MQTT发布器（无硬编码Topic，完全由调用方控制）
 * @note 仅封装发布逻辑，Topic/Payload/QoS均由外部传入
 */
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

    // // 设备状态发布（示例：业务层仅封装Payload构造，Topic仍由外部传入）
    // static bool publishDeviceStatus(const std::string& topic,
    //                                 float temp,
    //                                 int progress,
    //                                 int qos = 1,
    //                                 bool retained = false,
    //                                 bool force_sync = false) {
    //     // 仅封装Payload构造，Topic由调用方指定
    //     std::string payload = "{\"temp\":" + std::to_string(temp) +
    //                           ",\"progress\":" + std::to_string(progress) + "}";
    //     if (force_sync) {
    //         return publishSync(topic, payload, qos, retained);
    //     } else {
    //         return publish(topic, payload, qos, retained);
    //     }
    // }
};