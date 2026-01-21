
//
// Created by wave on 2026/1/8.
//
#include <iostream>
#include <algorithm>
#include <chrono>

#include "logger/logger.h"
#include "mqtt/thread.h"


MqttThread::MqttThread(const std::string &broker_ip, int broker_port, const std::string &client_id):m_broker_ip(broker_ip), m_broker_port(broker_port), m_client_id(client_id){
    m_client = std::make_unique<MqttClient>(broker_ip, broker_port, client_id);


    m_client->setMessageCallback([this](const std::string& topic, const std::string& payload) {
        this->onMessageReceived(topic, payload);
    });
}

MqttThread::~MqttThread() {
    stop();
}

bool MqttThread::start(bool auto_reconnect, int reconnect_interval) {
    if(m_running){
        LOG_ERROR("[MQTTThread] already start");
        return false;
    }


    m_auto_reconnect = auto_reconnect;
    m_reconnect_interval = reconnect_interval;
    m_running = true;


    m_worker_thread = std::make_unique<std::thread>([this]() {
        this->workerThread();
    });

    LOG_INFO("[MQTT Thread] thread start");
    return true;
}

void MqttThread::stop() {
    if(!m_running){
        return;
    }

    LOG_INFO("[MQTT Thread] thread stop");

    m_running = false;

    m_queue_cv.notify_all();

    if(m_worker_thread&&m_worker_thread->joinable()){
        m_worker_thread->join();
    }

    if(m_client){
        m_client->disconnect();
    }

    LOG_INFO("[MQTT Thread] thread stopped");
}

void MqttThread::workerThread() {
    LOG_INFO("[MQTT Thread] start worker thread");
    while (m_running) {
        try {
            bool was_connnected = m_connected.load();
            bool connected = handleConnection();


            if (connected && !was_connnected) {
                updateStatus("connected", true);

                std::lock_guard<std::mutex> lock(m_sub_mutex);
                for (const auto &sub: m_subscriptions) {
                    m_client->subscribe(sub.first, sub.second);
                }
            } else if (!connected && was_connnected) {
                updateStatus("disconnect", false);
            }

            if (connected) {
                handleMessageSending();
                m_client->yield(100);
            } else {
                if (m_auto_reconnect) {
                    updateStatus("[MQTT Thread]waite for reconnected...", false);
                    for (int i = 0; i < m_reconnect_interval * 10 && m_running; ++i) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                } else {
                    LOG_INFO("[MQTT Thread]connected failed");
                    break;
                }
            }

        } catch (const std::exception &e) {
            LOG_ERROR("[MQTT Thread] worker thread error: {}", e.what());
            updateStatus("error: " + std::string(e.what()), false);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    }
    LOG_INFO("[MQTT Thread] worker stopped");
}


bool MqttThread::handleConnection() {
    if(m_connected){
        return true;
    }

    updateStatus("connecting...", false);
    bool  connected = false;
    if(m_username.empty()){
        LOG_ERROR("[MQTT Thread] username is empty");
    } else{
        connected = m_client->connect(m_username, m_password);
    }

    m_connected = connected;
    return connected;
}


void MqttThread::handleMessageSending() {
    std::unique_lock<std::mutex> lock(m_queue_mutex);

    if (m_message_queue.empty() && !m_current_sync_publish) {
        lock.unlock();
        return;
    }

    if (m_current_sync_publish) {
        auto sync_msg = m_current_sync_publish;
        lock.unlock();

        bool success = m_client->publish(sync_msg->topic,
                                         sync_msg->payload,
                                         sync_msg->qos);

        std::lock_guard<std::mutex> sync_lock(m_sync_mutex);
        sync_msg->success = success;
        sync_msg->completed = true;
        sync_msg->cv.notify_one();

        m_current_sync_publish.reset();
        return;
    }

    while (!m_message_queue.empty() && m_connected) {
        auto msg = m_message_queue.front();
        m_message_queue.pop();

        lock.unlock();

        try {
            bool success = m_client->publish(msg.topic, msg.payload, msg.qos);
            if (!success) {
                LOG_ERROR("[MQTT Thread] failed to publish: {}", msg.topic);
                // TODO: 可以考虑将失败的消息重新加入队列,这里简单丢弃
            } else {
                LOG_INFO("[MQTT Thread] publish successfully");
            }
        } catch (...) {
            LOG_ERROR("[MQTT Thread] failed to publish message");
        }

        lock.lock();
    }
}


bool  MqttThread::publish(const std::string &topic, const std::string &payload, int qos, bool retained) {

    if(!m_running){
        LOG_ERROR("[MQTT Thread] mqtt client is not start");
        return false;
    }


    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_message_queue.emplace(topic, payload, qos, retained);
    //TODO: 唤醒条件变量
    m_queue_cv.notify_one();

    return true;
}


bool MqttThread::publishSync(const std::string &topic, const std::string &payload, int qos, int timeout_ms) {
    if (!m_running){
        LOG_ERROR("[MQTT Thread] not running");
        return false;
    }

    auto sync_msg = std::make_shared<SyncPublish>();
    sync_msg->topic = topic;
    sync_msg->payload = payload;
    sync_msg->qos = qos;
    sync_msg->completed = false;
    sync_msg->success = false;

    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);//TODO：这里为什么要锁定队列呢？数据并不在队列里面啊
        m_current_sync_publish = sync_msg;
        m_queue_cv.notify_one();
    }

    std::unique_lock<std::mutex> lock(m_sync_mutex);
    auto status = sync_msg->cv.wait_for(lock,
                                        std::chrono::milliseconds(timeout_ms),
                                        [&sync_msg]() { return sync_msg->completed; });



    if (!status) {
        LOG_ERROR("[MQTT Thread] timeout for sync message");
        std::lock_guard<std::mutex> queue_lock(m_queue_mutex);
        if (m_current_sync_publish == sync_msg) {
            m_current_sync_publish.reset();
        }

        return false;
    }

    return sync_msg->success;
}

bool MqttThread::subscribe(const std::string& topic, int qos) {
    std::lock_guard<std::mutex> lock(m_sub_mutex);

    auto it = std::find_if(m_subscriptions.begin(), m_subscriptions.end(),
                           [&topic](const auto& sub) { return sub.first == topic; });

    if (it == m_subscriptions.end()) {
        m_subscriptions.emplace_back(topic, qos);
    } else {
        it->second = qos;
    }

    if (m_connected) {
        return m_client->subscribe(topic, qos);
    }

    return true;
}


void MqttThread::onMessageReceived(const std::string& topic,
                                         const std::string& payload) {
    if (m_message_callback) {
        try {
            m_message_callback(topic, payload);
        } catch (const std::exception& e) {
            LOG_ERROR("[MQTT Thread] message callback failed : {}", e.what() );
        }
    }
}


void MqttThread::setMessageCallback(MessageCallback cb) {
    m_message_callback = cb;
}

void MqttThread::setStatusCallback(StatusCallback cb) {
    m_status_callback = cb;
}

void MqttThread::updateStatus(const std::string& status, bool connected) {
    if (m_status_callback) {
        try {
            m_status_callback(status, connected);
        } catch (const std::exception& e) {
            LOG_ERROR("[MQTT Thread] status callback error: : {}", e.what() );
        }
    }
}

bool MqttThread::isConnected() const {
    return m_connected;
}

size_t MqttThread::queueSize() const {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    return m_message_queue.size();
}

void MqttThread::clearQueue() {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    while (!m_message_queue.empty()) {
        m_message_queue.pop();
    }
}

void MqttThread::setAuth(const std::string& username, const std::string& password) {
    m_username = username;
    m_password = password;
}