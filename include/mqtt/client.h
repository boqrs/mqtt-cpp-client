//
// Created by wave on 2026/1/27.
//

#pragma once
#include <string>
#include <functional>
#include <memory>
#include <MQTTClient.h>
#include "i_mqtt_client.h"

// 消息回调类型定义
using MqttMessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
using MqttStatusCallback = std::function<void(bool connected, const std::string& reason)>;


class MqttClient : public IMqttClient{
public:
      MqttClient(const std::string& broker_ip, int broker_port, const std::string& client_id,
              const std::string& username = "", const std::string& password = "", bool auto_reconnect = true);
    ~MqttClient();

    // 禁止拷贝
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    // 连接MQTT服务器（支持用户名密码认证）
    bool connect() override;

    // 断开连接
    void disconnect() override;
    // 发布消息（QoS 0/1/2，支持保留消息）
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retained = false)override;
    // 订阅主题
    bool subscribe(const std::string& topic, int qos = 0) override;
    // 取消订阅
    bool unsubscribe(const std::string& topic) override;
    // 处理网络事件（必须周期性调用，否则无法接收消息）
    void yield(int timeout_ms = 100)override;
    // 检查连接状态
    bool isConnected() const override;

    // 设置回调函数
    void setMessageCallback(MqttMessageCallback cb) override;
    void setStatusCallback(MqttStatusCallback cb) override;

private:
    // 底层消息回调（适配Paho C库）
    static int onMessageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);
    // 连接丢失回调
    static void onConnectionLost(void* context, char* cause);

    std::string m_broker_uri;  // tcp://ip:port
    std::string m_client_id;
    MQTTClient m_client = nullptr;
    bool m_connected = false;
    std::string m_username;
    std::string m_password;
    bool m_auto_reconnect;
    MqttMessageCallback m_msg_callback;
    MqttStatusCallback m_status_callback;
};