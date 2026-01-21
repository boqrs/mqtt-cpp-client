//
// Created by wave on 2026/1/8.
#include <cstring>
#include <MQTTReasonCodes.h>  // 包含MQTTReasonCodes定义

#include "mqtt/client.h"
#include "logger/logger.h"

// 消息到达回调函数
int Impl::messageArrived(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    Impl* impl = static_cast<Impl*>(context);
    if (!impl || !impl->user_cb) {
        return 1;
    }

    std::string topic(topicName);
    std::string payload(static_cast<char*>(message->payload), message->payloadlen);

    LOG_INFO("[MQTT] received message: topic={}, size={} bytes", topic, message->payloadlen);

    // 调用用户回调
    impl->user_cb(topic, payload);

    // 释放消息
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

Impl::Impl(const std::string& ip, int port, const std::string& id)
    : broker_ip(ip), broker_port(port), client_id(id) {
    // 构建broker URI
    broker_uri = "tcp://" + broker_ip + ":" + std::to_string(broker_port);
    LOG_DEBUG("[MQTT] Creating MQTT client: uri={}, client_id={}", broker_uri, client_id);
}

Impl::~Impl() {
    LOG_DEBUG("[MQTT] Destroying MQTT client");
    if (client) {
        disconnect();
        MQTTClient_destroy(&client);
        client = nullptr;
    }
}

bool Impl::connect(const std::string &username, const std::string &password) {
    if (connected) {
        LOG_ERROR("[MQTT] already connected");
        return true;
    }

    LOG_INFO("[MQTT] Attempting to connect to {} with client ID: {}", broker_uri, client_id);

    // 创建客户端
    int rc = MQTTClient_create(&client, broker_uri.c_str(), client_id.c_str(),
                               MQTTCLIENT_PERSISTENCE_NONE, nullptr);

    LOG_DEBUG("[MQTT] MQTTClient_create returned: {}", rc);

    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("[MQTT] failed to create client, rc={}", rc);
        return false;
    }

    if (!client) {
        LOG_ERROR("[MQTT] client is null after creation");
        return false;
    }

    // 设置连接选项
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;  // 清理会话

    // 尝试使用MQTT 5.0，但先回退到3.1.1以确保连接
    #if defined(MQTTVERSION_5) && MQTTVERSION_5 == 5
        conn_opts.MQTTVersion = MQTTVERSION_5;
        LOG_DEBUG("[MQTT] Using MQTT 5.0");
    #else
        conn_opts.MQTTVersion = 4;  // MQTT 3.1.1
        LOG_DEBUG("[MQTT] Using MQTT 3.1.1 (fallback)");
    #endif

    // 设置用户名和密码
    if (!username.empty()) {
        conn_opts.username = username.c_str();
        LOG_DEBUG("[MQTT] Using username: {}", username);
        if (!password.empty()) {
            conn_opts.password = password.c_str();
            LOG_DEBUG("[MQTT] Using password (hidden)");
        }
    }

    // 设置回调函数
    MQTTClient_setCallbacks(client, this,
                           nullptr,  // 连接丢失回调
                           Impl::messageArrived,  // 消息到达回调
                           nullptr);  // 消息传递完成回调

    // 连接
    LOG_DEBUG("[MQTT] Attempting MQTTClient_connect");
    rc = MQTTClient_connect(client, &conn_opts);
    LOG_DEBUG("[MQTT] MQTTClient_connect returned: {}", rc);

    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("[MQTT] failed to connect: rc={}", rc);

        // 尝试回退到MQTT 3.1.1
        if (conn_opts.MQTTVersion != 4) {
            LOG_INFO("[MQTT] Trying fallback to MQTT 3.1.1");
            conn_opts.MQTTVersion = 4;  // MQTT 3.1.1

            rc = MQTTClient_connect(client, &conn_opts);
            LOG_DEBUG("[MQTT] MQTTClient_connect (fallback) returned: {}", rc);

            if (rc != MQTTCLIENT_SUCCESS) {
                LOG_ERROR("[MQTT] fallback connection also failed: rc={}", rc);
                MQTTClient_destroy(&client);
                client = nullptr;
                return false;
            }
        } else {
            MQTTClient_destroy(&client);
            client = nullptr;
            return false;
        }
    }

    connected = true;
    LOG_INFO("[MQTT] connected successfully");
    return true;
}

bool Impl::publish(const std::string &topic, const std::string &payload, int qos) const {
    if (!connected || !client) {
        LOG_ERROR("[MQTT] not connected");
        return false;
    }

    if (qos < 0) qos = 0;
    if (qos > 2) qos = 2;

    LOG_DEBUG("[MQTT] Publishing to topic: {}, QoS: {}, payload size: {}",
              topic, qos, payload.size());

    // 创建消息
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = const_cast<char*>(payload.c_str());
    pubmsg.payloadlen = static_cast<int>(payload.length());
    pubmsg.qos = qos;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;

    // 发布消息
    int rc = MQTTClient_publishMessage(client, topic.c_str(), &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("[MQTT] failed to publish: rc={}", rc);
        return false;
    }

    // 等待发布完成
    rc = MQTTClient_waitForCompletion(client, token, 5000);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("[MQTT] publish timeout or failed: rc={}", rc);
        return false;
    }

    return true;
}

bool Impl::subscribe(const std::string &topic, int qos, bool no_local) const {
    if (!connected || !client) {
        LOG_ERROR("[MQTT] not connected");
        return false;
    }

    if (qos < 0) qos = 0;
    if (qos > 2) qos = 2;

    LOG_DEBUG("[MQTT] Subscribing to topic: {}, QoS: {}, noLocal: {}",
              topic, qos, no_local);

    // 使用标准订阅函数
    int rc = MQTTClient_subscribe(client, topic.c_str(), qos);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_ERROR("[MQTT] failed to subscribe: rc={}", rc);
        return false;
    }

    LOG_INFO("[MQTT] subscribed to {} (QoS: {}, noLocal: {})", topic, qos, no_local);
    return true;
}

void Impl::yield(int timeout_ms) {
    if (!connected || !client) return;

    // 调用yield让客户端处理网络流量
    MQTTClient_yield();
}

void Impl::disconnect() {
    if (client && connected) {
        LOG_DEBUG("[MQTT] Disconnecting");
        MQTTClient_disconnect(client, 1000);
        connected = false;
        LOG_INFO("[MQTT] disconnected");
    }
}