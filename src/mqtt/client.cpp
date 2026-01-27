//
// Created by wave on 2026/1/27.
//
#include "mqtt/client.h"
#include <cstring>
#include <iostream>
#include "logger/logger.h"

MqttClient::MqttClient(const std::string& broker_ip, int broker_port, const std::string& client_id)
    : m_client_id(client_id) {
    m_broker_uri = "tcp://" + broker_ip + ":" + std::to_string(broker_port);
}

MqttClient::~MqttClient() {
    disconnect();
    if (m_client) {
        MQTTClient_destroy(&m_client);
    }
}

bool MqttClient::connect(const std::string& username, const std::string& password) {
    if (m_connected) {
        LOG_ERROR("Already connected to {}", m_broker_uri.c_str());
        return true;
    }

    // 创建客户端实例
    int rc = MQTTClient_create(&m_client, m_broker_uri.c_str(), m_client_id.c_str(),
                               MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Create client failed, rc={}", rc);
        return false;
    }

    // 配置连接参数
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.MQTTVersion = MQTTVERSION_3_1_1;  // 兼容主流MQTT服务器

    // 设置用户名密码
    if (!username.empty()) {
        conn_opts.username = username.c_str();
        conn_opts.password = password.empty() ? nullptr : password.c_str();
    }

    // 注册回调
    MQTTClient_setCallbacks(m_client, this, onConnectionLost, onMessageArrived, nullptr);

    // 发起连接
    rc = MQTTClient_connect(m_client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Connect to {} failed, rc={}", m_broker_uri.c_str(), rc);
        MQTTClient_destroy(&m_client);
        m_client = nullptr;
        return false;
    }

    m_connected = true;
    LOG_INFO("Connected to {} successfully", m_broker_uri.c_str());
    if (m_status_callback) {
        m_status_callback(true, "Connected");
    }
    return true;
}

void MqttClient::disconnect() {
    if (m_connected && m_client) {
        MQTTClient_disconnect(m_client, 1000);
        m_connected = false;
        LOG_INFO("Disconnected from {}", m_broker_uri.c_str());
        if (m_status_callback) {
            m_status_callback(false, "Disconnected manually");
        }
    }
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
    if (!m_connected || !m_client) {
        LOG_ERROR("Publish failed: not connected");
        return false;
    }

    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = const_cast<char*>(payload.c_str());
    msg.payloadlen = static_cast<int>(payload.size());
    msg.qos = qos;
    msg.retained = retained ? 1 : 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(m_client, topic.c_str(), &msg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Publish to {} failed, rc: {}", topic.c_str(), rc);
        return false;
    }

    // 等待发布确认（QoS>0时必要）
    rc = MQTTClient_waitForCompletion(m_client, token, 5000);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Publish confirm timeout, rc= {}", rc);
        return false;
    }

    LOG_INFO("Publish to {} success {}", topic.c_str(), payload.size());
    return true;
}

bool MqttClient::subscribe(const std::string& topic, int qos) {
    if (!m_connected || !m_client) {
        LOG_ERROR("Subscribe failed: not connected");
        return false;
    }

    int rc = MQTTClient_subscribe(m_client, topic.c_str(), qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Subscribe to {} failed, rc={}", topic.c_str(), rc);
        return false;
    }

    LOG_INFO("Subscribe to {} success {}", topic.c_str(), qos);
    return true;
}

bool MqttClient::unsubscribe(const std::string& topic) {
    if (!m_connected || !m_client) {
        return false;
    }

    int rc = MQTTClient_unsubscribe(m_client, topic.c_str());
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("Unsubscribe {} failed, rc={}", topic.c_str(), rc);
        return false;
    }
    return true;
}

void MqttClient::yield(int timeout_ms) {
    if (m_connected && m_client) {
        MQTTClient_yield();  // 处理网络事件，接收消息
    }
}

int MqttClient::onMessageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    MqttClient* client = static_cast<MqttClient*>(context);
    if (!client || !client->m_msg_callback) {
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    // 解析消息
    std::string topic(topicName);
    std::string payload(static_cast<char*>(message->payload), message->payloadlen);
    LOG_INFO("Received message: topic={}, size={}", topic.c_str(), message->payloadlen);

    // 调用业务回调
    client->m_msg_callback(topic, payload);

    // 释放资源
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void MqttClient::onConnectionLost(void* context, char* cause) {
    MqttClient* client = static_cast<MqttClient*>(context);
    if (client) {
        client->m_connected = false;
        LOG_ERROR("Connection lost: {}", cause ? cause : "Unknown reason");
        if (client->m_status_callback) {
            client->m_status_callback(false, cause ? cause : "Unknown reason");
        }
    }
}