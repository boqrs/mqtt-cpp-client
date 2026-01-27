//
// Created by wave on 2026/1/9.
//
#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <condition_variable>
#include "client.h"

struct MqttMessage{
    std::string  topic;
    std::string payload;
    int qos;
    bool  retained;

    MqttMessage(const std::string& topic, const std::string& payload, bool  r = false, int q = 0):topic(topic), payload(payload), qos(q), retained(r){
    };
};

class MqttThread{
public:
    using MessageCallback = std::function<void(const std::string& topic,
                                               const std::string& payload)>;

    using StatusCallback = std::function<void(const std::string& status,
                                              bool connected)>;


    MqttThread(const std::string& broker_ip,
                     int broker_port,
                     const std::string& client_id);

    ~MqttThread();

    //禁止使用拷贝
    MqttThread(const MqttThread&) = delete;
    //禁止使用赋值
    MqttThread& operator=(const MqttThread&) = delete;

    bool start(bool auto_reconnect = true, int reconnect_interval = 5);
    void stop();

    bool publish(const std::string& topic,
                 const std::string& payload,
                 int qos = 0,
                 bool retained = false);

    bool publishSync(const std::string& topic,
                     const std::string& payload,
                     int qos = 0,
                     int timeout_ms = 5000);

    bool subscribe(const std::string& topic, int qos = 0);

    void setMessageCallback(MessageCallback cb);

    void setStatusCallback(StatusCallback cb);

    bool isConnected() const;

    size_t queueSize() const;

    void clearQueue();

    void setAuth(const std::string& username, const std::string& password);
private:
    void workerThread();
    bool handleConnection();
    void handleMessageSending();
    void onMessageReceived(const std::string& topic, const std::string& payload);
    void updateStatus(const std::string& status, bool connected);

    std::unique_ptr<MqttClient> m_client;

    std::string m_username;
    std::string m_password;

    std::unique_ptr<std::thread> m_worker_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_auto_reconnect{true};
    std::atomic<int> m_reconnect_interval{5};

    std::queue<MqttMessage> m_message_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;


    struct SyncPublish {
        std::string topic;
        std::string payload;
        int qos;
        bool completed;
        bool success;
        std::condition_variable cv;
    };

    std::shared_ptr<SyncPublish> m_current_sync_publish;
    std::mutex m_sync_mutex;

    MessageCallback m_message_callback;
    StatusCallback m_status_callback;

    std::vector<std::pair<std::string, int>> m_subscriptions;
    mutable std::mutex m_sub_mutex;
};