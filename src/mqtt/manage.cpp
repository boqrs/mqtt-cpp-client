//
// Created by wave on 2026/1/27.
//

#include <chrono>
#include <random>
#include <sstream>

#include "mqtt/manage.h"
#include  "logger/logger.h"

// 生成随机客户端ID（为空时使用）
static std::string generateClientId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    std::stringstream ss;
    ss << "mqtt_client_" << dis(gen);
    return ss.str();
}

bool MqttManager::init(const std::string& broker_ip, int broker_port,
                       const std::string& client_id, const std::string& username,
                       const std::string& password, bool auto_reconnect, int reconnect_interval) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        LOG_ERROR("MqttManager already initialized");
        return false;
    }

    m_broker_ip = broker_ip;
    m_broker_port = broker_port;
    m_client_id = client_id.empty() ? generateClientId() : client_id;
    m_username = username;
    m_password = password;
    m_auto_reconnect = auto_reconnect;
    m_reconnect_interval = reconnect_interval;

    // 创建基础客户端
    m_client = std::make_unique<MqttClient>(m_broker_ip, m_broker_port, m_client_id);
    m_client->setMessageCallback([this](const std::string& topic, const std::string& payload) {
        if (m_msg_callback) {
            m_msg_callback(topic, payload);
        }
    });
    m_client->setStatusCallback([this](bool connected, const std::string& reason) {
        m_connected = connected;
        if (m_status_callback) {
            m_status_callback(connected, reason);
        }
    });

    return true;
}

bool MqttManager::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        return true;
    }

    m_running = true;
    m_worker_thread = std::make_unique<std::thread>(&MqttManager::workerThread, this);
    return true;
}

void MqttManager::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        return;
    }

    m_running = false;
    m_queue_cv.notify_all();  // 唤醒队列
    if (m_worker_thread->joinable()) {
        m_worker_thread->join();
    }

    if (m_client) {
        m_client->disconnect();
    }

    // 清空队列
    std::lock_guard<std::mutex> q_lock(m_queue_mutex);
    while (!m_publish_queue.empty()) {
        m_publish_queue.pop();
    }
}

void MqttManager::workerThread() {
    LOG_INFO("MqttManager worker thread started");
    while (m_running) {
        try {
            // 确保连接有效
            if (!m_connected) {
                if (!reconnect()) {
                    // 重连失败，等待后重试
                    std::this_thread::sleep_for(std::chrono::seconds(m_reconnect_interval));
                    continue;
                }
                restoreSubscriptions();  // 恢复订阅
            }

            // 处理消息队列
            processMessageQueue();

            // 处理网络事件（接收消息）
            m_client->yield(100);

            // 短暂休眠，降低CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } catch (const std::exception& e) {
            LOG_ERROR("Worker thread error: %s", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    LOG_INFO("MqttManager worker thread stopped");
}

void MqttManager::processMessageQueue() {
    std::unique_lock<std::mutex> q_lock(m_queue_mutex);
    while (!m_publish_queue.empty() && m_connected) {
        auto task = m_publish_queue.front();
        m_publish_queue.pop();
        q_lock.unlock();

        // 发布消息
        m_client->publish(task.topic, task.payload, task.qos, task.retained);

        q_lock.lock();
    }
    q_lock.unlock();
}

bool MqttManager::publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
    if (!m_running) {
        LOG_ERROR("MqttManager not running");
        return false;
    }

    std::lock_guard<std::mutex> q_lock(m_queue_mutex);
    m_publish_queue.push({topic, payload, qos, retained});
    m_queue_cv.notify_one();  // 唤醒工作线程处理
    return true;
}

bool MqttManager::publishSync(const std::string& topic, const std::string& payload, int qos,
                             bool retained, int timeout_ms) {
    if (!m_running || !m_connected) {
        LOG_ERROR("Publish sync failed: not connected");
        return false;
    }

    // 直接调用客户端同步发布
    return m_client->publish(topic, payload, qos, retained);
}

bool MqttManager::subscribe(const std::string& topic, int qos) {
    std::lock_guard<std::mutex> sub_lock(m_sub_mutex);
    // 检查是否已订阅（去重）
    bool found = false;
    for (auto& sub : m_subscriptions) { // 移除const
        if (sub.first == topic) {
            found = true;
            if (sub.second != qos) {
                if (m_connected) {
                    m_client->unsubscribe(topic);
                    if (!m_client->subscribe(topic, qos)) {
                        LOG_ERROR("Failed to update subscription QoS: %s", topic.c_str());
                        return false;
                    }
                }
                sub.second = qos; // 现在可以赋值了
            }
            break;
        }
    }

    if (!found) {
        m_subscriptions.emplace_back(topic, qos);
        if (m_connected) {
            return m_client->subscribe(topic, qos);
        }
    }

    return true;
}

void MqttManager::restoreSubscriptions() {
    std::lock_guard<std::mutex> lock(m_sub_mutex);
    if (m_subscriptions.empty()) {
        LOG_INFO("No subscriptions to restore");
        return;
    }

    LOG_INFO("Restoring %zu subscriptions...", m_subscriptions.size());
    for (const auto& sub : m_subscriptions) {
        if (m_client->subscribe(sub.first, sub.second)) {
            LOG_INFO("Restored subscription: %s (QoS=%d)", sub.first.c_str(), sub.second);
        } else {
            LOG_ERROR("Failed to restore subscription: %s", sub.first.c_str());
        }
    }
}

bool MqttManager::reconnect() {
    LOG_INFO("Trying to reconnect to %s:%d", m_broker_ip.c_str(), m_broker_port);
    bool ret = m_client->connect(m_username, m_password);
    if (ret) {
        m_connected = true;
        LOG_INFO("Reconnected successfully");
    } else {
        m_connected = false;
        LOG_ERROR("Reconnect failed");
    }
    return ret;
}

bool MqttManager::unsubscribe(const std::string& topic) {
    std::lock_guard<std::mutex> sub_lock(m_sub_mutex);
    for (auto it = m_subscriptions.begin(); it != m_subscriptions.end(); ++it) {
        if (it->first == topic) {
            m_subscriptions.erase(it);
            // 如果已连接，立即取消订阅
            if (m_connected) {
                return m_client->unsubscribe(topic);
            }
            return true;
        }
    }
    return false;
}