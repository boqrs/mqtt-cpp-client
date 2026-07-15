// /workspace/include/mqtt/i_mqtt_client.h
#pragma once

#include <string>
#include <functional>
#include <memory>

// 消息回调类型定义
using MqttMessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
// 状态回调类型定义 (connected, reason)
using MqttStatusCallback = std::function<void(bool connected, const std::string& reason)>;

/**
 * @brief 抽象 MQTT 客户端接口
 * 定义了所有具体 MQTT 客户端（如 Paho MQTT 或 AWS IoT Core MQTT）必须实现的功能。
 */
class IMqttClient {
public:
    // 虚析构函数，确保派生类析构时能正确调用
    virtual ~IMqttClient() = default;

    // 连接 MQTT 服务器
    virtual bool connect() = 0;

    // 断开连接
    virtual void disconnect() = 0;

    // 发布消息
    virtual bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retained = false) = 0;

    // 订阅主题
    virtual bool subscribe(const std::string& topic, int qos = 0) = 0;

    // 取消订阅
    virtual bool unsubscribe(const std::string& topic) = 0;

    // 处理网络事件（对于某些库可能需要周期性调用）
    virtual void yield(int timeout_ms = 100) = 0;

    // 检查连接状态
    virtual bool isConnected() const = 0;

    // 设置消息回调函数
    virtual void setMessageCallback(MqttMessageCallback cb) = 0;

    // 设置状态回调函数
    virtual void setStatusCallback(MqttStatusCallback cb) = 0;
};