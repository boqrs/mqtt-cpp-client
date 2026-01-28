//
// Created by wave on 2026/1/27.
//

#pragma once
#include "client.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <queue>

class MqttManager {
public:
    // 单例实例（全局唯一）
    static MqttManager& getInstance() {
        static MqttManager instance;
        return instance;
    }

    // 禁止拷贝
    MqttManager(const MqttManager&) = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    bool init(const std::string& broker_ip, int broker_port = 1883,
              const std::string& client_id = "", const std::string& username = "",
              const std::string& password = "", bool auto_reconnect = true,
              int reconnect_interval = 5);

    // 启动连接（后台线程）
    bool start();
    // 停止连接
    void stop();

    // 异步发布消息（非阻塞）
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retained = false);
    // 同步发布消息（阻塞，带超时）
    bool publishSync(const std::string& topic, const std::string& payload, int qos = 0,
                     bool retained = false, int timeout_ms = 5000);

    // 订阅主题（断线后自动恢复）
    bool subscribe(const std::string& topic, int qos = 0);

    // 取消订阅
    bool unsubscribe(const std::string& topic);

    // 设置全局回调
    void setMessageCallback(MqttMessageCallback cb) { m_msg_callback = std::move(cb); }
    void setStatusCallback(MqttStatusCallback cb) { m_status_callback = std::move(cb); }

    void setDefaultConfig(int default_qos, bool default_retained, int publish_timeout_ms) {
        m_default_qos = default_qos;
        m_default_retained = default_retained;
        m_publish_timeout_ms = publish_timeout_ms;
    }
    // 检查连接状态
    bool isConnected() const { return m_connected; }

    void setPublishQueueMaxSize(size_t max_size) {
        std::lock_guard<std::mutex> q_lock(m_queue_mutex);
        m_queue_max_size = max_size;
    }
private:
    MqttManager() = default;
    ~MqttManager() { stop(); }

    // 后台工作线程（处理连接、重连、消息发送）
    void workerThread();
    // 处理自动重连
    bool reconnect();
    // 恢复断线前的订阅
    void restoreSubscriptions();
    // 处理消息队列（异步发布）
    void processMessageQueue();

    // 消息队列项（异步发布用）
    struct PublishTask {
        std::string topic;
        std::string payload;
        int qos;
        bool retained;
    };

    // 配置参数
    std::string m_broker_ip;
    int m_broker_port = 1883;
    std::string m_client_id;
    std::string m_username;
    std::string m_password;
    bool m_auto_reconnect = true;
    int m_reconnect_interval = 5;

    // 核心对象
    std::unique_ptr<MqttClient> m_client;
    std::unique_ptr<std::thread> m_worker_thread;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_connected = false;

    // 线程安全相关
    std::mutex m_mutex;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    std::queue<PublishTask> m_publish_queue;  // 异步发布队列

    // 订阅列表（断线恢复用）
    std::vector<std::pair<std::string, int>> m_subscriptions;
    std::mutex m_sub_mutex;

    // 回调函数
    MqttMessageCallback m_msg_callback;
    MqttStatusCallback m_status_callback;


    // 默认配置（新增）
    int m_default_qos = 1;
    bool m_default_retained = false;
    int m_publish_timeout_ms = 5000;
    int m_queue_max_size;
};