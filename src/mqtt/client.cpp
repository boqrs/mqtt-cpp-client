//
// Created by wave on 2026/1/8.
//
#include <utility>
#include "mqtt/client.h"

MqttClient::MqttClient(const std::string& ip, int port, const std::string& client_id)
    : pimpl_(std::make_unique<Impl>(ip, port, client_id)) {}

MqttClient::~MqttClient() = default;

MqttClient::MqttClient(MqttClient&& other) noexcept = default;
MqttClient& MqttClient::operator=(MqttClient &&) noexcept = default;

bool MqttClient::connect(const std::string& username, const std::string& password) const {
    return pimpl_->connect(username, password);
}

void MqttClient::disconnect() const {
    pimpl_->disconnect();
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos) const {
    return pimpl_->publish(topic, payload, qos);
}

bool MqttClient::subscribe(const std::string& topic, int qos, bool no_local) const {
    return pimpl_->subscribe(topic, qos, no_local);
}

void MqttClient::setMessageCallback(messageCallback cb) const {
    pimpl_->user_cb = std::move(cb);
}

void MqttClient::yield(int timeout_ms) const {
    pimpl_->yield(timeout_ms);
}

bool MqttClient::isConnected() const {
    return pimpl_->connected;
}