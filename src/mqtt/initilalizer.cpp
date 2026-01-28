//
// Created by wave on 2026/1/27.
//

#include "mqtt/manage.h"
#include "mqtt/initializer.h"
#include "config.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <iostream>

#include "logger/logger.h"
#include "protocol/initialize.h"
// 单例实现
MqttInitializer& MqttInitializer::getInstance() {
    static MqttInitializer instance;
    return instance;
}

MqttInitializer::MqttInitializer() {
    MqttManager::getInstance().setStatusCallback(
        [this](bool connected, const std::string& reason) {
            this->onConnectionStatusChanged(connected, reason);
        }
    );
}

bool MqttInitializer::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        LOG_INFO("MQTT client already initialized");
        return true;
    }

    // 宏兜底：所有CMake宏未定义时使用默认值
#ifndef MQTT_BROKER_IP
#define MQTT_BROKER_IP "127.0.0.1"
#endif
#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif
#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID ""
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_AUTO_RECONNECT
#define MQTT_AUTO_RECONNECT 1
#endif
#ifndef MQTT_MAX_RETRY_COUNT
#define MQTT_MAX_RETRY_COUNT 3
#endif
#ifndef MQTT_BASE_BACKOFF_MS
#define MQTT_BASE_BACKOFF_MS 1000
#endif
#ifndef MQTT_MAX_BACKOFF_MS
#define MQTT_MAX_BACKOFF_MS 10000
#endif
#ifndef MQTT_DEFAULT_QOS
#define MQTT_DEFAULT_QOS 1
#endif
#ifndef MQTT_DEFAULT_RETAINED
#define MQTT_DEFAULT_RETAINED 0
#endif
#ifndef MQTT_PUBLISH_TIMEOUT_MS
#define MQTT_PUBLISH_TIMEOUT_MS 5000
#endif

#ifndef MQTT_PUBLISH_QUEUE_MAX_SIZE
#define MQTT_PUBLISH_QUEUE_MAX_SIZE 1000
#endif

    // 加载配置（现在所有宏都有定义）
    const std::string broker_ip = MQTT_BROKER_IP;
    const int broker_port = MQTT_BROKER_PORT;
    const std::string client_id = MQTT_CLIENT_ID;
    const std::string username = MQTT_USERNAME;
    const std::string password = MQTT_PASSWORD;
    const bool auto_reconnect = MQTT_AUTO_RECONNECT;
    const int max_retry = MQTT_MAX_RETRY_COUNT;
    const int base_backoff_ms = MQTT_BASE_BACKOFF_MS;

    // 解析订阅主题（现在不会报MQTT_SUBSCRIBE_TOPICS未定义）
    m_subscribe_topics = parseMqttSubscribeTopics();

    // 校验配置
    if (broker_ip.empty()) {
        LOG_ERROR("MQTT broker IP is empty");
        return false;
    }
    if (broker_port <= 0 || broker_port > 65535) {
        LOG_ERROR("Invalid MQTT broker port: {}", broker_port);
        return false;
    }

    // 初始化MQTT管理器
    LOG_INFO("Initializing MQTT manager: {}:{}, client_id={}",
             broker_ip.c_str(), broker_port,
             client_id.empty() ? "auto" : client_id.c_str());

    if (!MqttManager::getInstance().init(
            broker_ip, broker_port, client_id, username, password,
            auto_reconnect, base_backoff_ms / 1000
        )) {
        LOG_ERROR("MQTT manager init failed");
        return false;
    }

    // 设置默认配置
    MqttManager::getInstance().setDefaultConfig(
        MQTT_DEFAULT_QOS,
        MQTT_DEFAULT_RETAINED,
        MQTT_PUBLISH_TIMEOUT_MS
    );

    // 启动管理器
    if (!MqttManager::getInstance().start()) {
        LOG_ERROR("MQTT manager start failed");
        return false;
    }

    // 修复lambda警告：移除未使用的max_retry捕获
    LOG_INFO("Waiting for MQTT connection (max retry: {})", max_retry);
    std::unique_lock<std::mutex> cv_lock(m_cv_mutex);
    m_cv.wait(cv_lock, [this]() { // 移除max_retry捕获
        // 显式使用类内的m_current_retry和全局宏MQTT_MAX_RETRY_COUNT
        return m_connected || (m_current_retry >= MQTT_MAX_RETRY_COUNT);
    });

    // 检查连接状态
    if (!m_connected) {
        LOG_ERROR("MQTT connection failed after {} retries", max_retry);
        MqttManager::getInstance().stop();
        return false;
    }

    // 订阅主题
    if (!subscribeTopics()) {
        LOG_ERROR("Partial topics subscribe failed");
    }

     MqttManager::getInstance().setMessageCallback([this](const std::string& topic, const std::string& payload) {
         // 直接调用init中的解析分发接口，命令自动入队+独立线程执行
           bool ok = swan::init::dispatchMqttCommand(topic, payload);
           if (!ok) {
               LOG_WARN("Failed to dispatch MQTT command, topic: {}", topic);
           }
    });
    MqttManager::getInstance().setPublishQueueMaxSize(MQTT_PUBLISH_QUEUE_MAX_SIZE);
    m_initialized = true;
    LOG_INFO("MQTT init success: subscribed {} topics", m_subscribe_topics.size());
    return true;
}

// 其他函数保持不变，仅补充宏兜底（已在上面处理）
void MqttInitializer::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) {
        LOG_INFO("MQTT not initialized, skip shutdown");
        return;
    }

    MqttManager::getInstance().stop();
    m_initialized = false;
    m_connected = false;
    m_current_retry = 0;
    m_subscribe_topics.clear();

    LOG_INFO("MQTT shutdown success");
}

bool MqttInitializer::reconnectWithBackoff(int retry_count, int max_retry) {
    int backoff_ms = MQTT_BASE_BACKOFF_MS * (1 << (retry_count - 1));
    backoff_ms = std::min(backoff_ms, MQTT_MAX_BACKOFF_MS);

    LOG_INFO("Retry {}/{}: backoff {}ms", retry_count, max_retry, backoff_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));

    return MqttManager::getInstance().init(
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT,
        MQTT_CLIENT_ID,
        MQTT_USERNAME,
        MQTT_PASSWORD,
        MQTT_AUTO_RECONNECT,
        MQTT_BASE_BACKOFF_MS / 1000
    ) && MqttManager::getInstance().start();
}

void MqttInitializer::onConnectionStatusChanged(bool connected, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_cv_mutex);
    m_connected = connected;

    if (connected) {
        LOG_INFO("MQTT connected: {}", reason.c_str());
        m_cv.notify_one();
    } else {
        LOG_ERROR("MQTT disconnected: {}", reason.c_str());

        if (m_current_retry < MQTT_MAX_RETRY_COUNT) {
            m_current_retry++;
            LOG_INFO("Reconnect attempt {}/{}", m_current_retry, MQTT_MAX_RETRY_COUNT);

            std::thread retry_thread([this]() {
                this->reconnectWithBackoff(m_current_retry, MQTT_MAX_RETRY_COUNT);
            });
            retry_thread.detach();
        } else {
            LOG_ERROR("Max retry reached");
            m_cv.notify_one();
        }
    }
}

bool MqttInitializer::subscribeTopics() {
    if (m_subscribe_topics.empty()) {
        LOG_INFO("No topics to subscribe");
        return true;
    }

    bool all_success = true;
    for (const auto& [topic, qos] : m_subscribe_topics) {
        if (!MqttManager::getInstance().subscribe(topic, qos)) {
            LOG_ERROR("Subscribe failed: {} (QoS={})", topic.c_str(), qos);
            all_success = false;
        } else {
            LOG_INFO("Subscribe success: {} (QoS={})", topic.c_str(), qos);
        }
    }

    return all_success;
}

// 全局函数
bool initMqtt() {
    return MqttInitializer::getInstance().initialize();
}

void shutdownMqtt() {
    MqttInitializer::getInstance().shutdown();
}