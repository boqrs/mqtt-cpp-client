//
// Created by wave on 2026/1/8.
//
#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <memory>

// 使用高级API
#include <MQTTClient.h>

using messageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

struct Impl {
    MQTTClient client = nullptr;
    std::string broker_ip;
    int broker_port = 1883;
    std::string client_id;
    std::string broker_uri;  // 格式: tcp://ip:port

    messageCallback user_cb;

    bool connected = false;

    Impl(const std::string& ip, int port, const std::string& id);
    ~Impl();

    bool connect(const std::string& username = "",
                 const std::string& password = "");

    bool publish(const std::string& topic,
                 const std::string& message,
                 int qos = 0) const;

    bool subscribe(const std::string& topic, int qos = 0, bool no_local = true) const;

    void yield(int timeout_ms = 100);
    void disconnect();

private:
    static int messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message);
};

class MqttClient {
public:
    MqttClient(const std::string& ip, int port, const std::string& client_id);
    ~MqttClient();

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    MqttClient(MqttClient&&) noexcept;
    MqttClient& operator=(MqttClient&&) noexcept;

    bool connect(const std::string& username = "", const std::string& password = "") const;
    void disconnect() const;
    bool publish(const std::string& topic, const std::string& payload, int qos = 0) const;
    bool subscribe(const std::string& topic, int qos = 0, bool no_local = true) const;
    void setMessageCallback(messageCallback cb) const;
    void yield(int timeout_ms = 100) const;
    bool isConnected() const;

private:
    std::unique_ptr<Impl> pimpl_;
};