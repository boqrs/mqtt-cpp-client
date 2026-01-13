//
// Created by wave on 2026/1/8.
//
#include <iostream>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mqtt/client.h"

MqttClient::MqttClient(const std::string &ip, int port, const std::string &client_id):pimpl_(std::make_unique<Impl>(ip, port, client_id)) {
    std::cout<<"[MQTT] 初始化客户端: "<<client_id<<" ip: "<<ip<<" port: "<<port<<std::endl;
}

MqttClient::~MqttClient()=default;
MqttClient::MqttClient(MqttClient&& other) noexcept = default;
MqttClient& MqttClient::operator=(MqttClient &&) noexcept = default;

bool  MqttClient::connect(const std::string &username, const std::string &password) {
 return pimpl_->connect(username, password);
}

bool MqttClient::publish(const std::string &topic, const std::string &payload, int qos) {
    return pimpl_->publish(topic, payload, qos);
}

bool MqttClient::subscribe(const std::string &topic, int qos) {
    return pimpl_->subscribe(topic, qos);
}

void MqttClient::disconnect() {
    pimpl_->disconnect_internal();
}

void MqttClient::setMessageCallback(messageCallback cb) {
    pimpl_->user_cb = std::move(cb);
}

void MqttClient::yield(int timeout_ms) {
    pimpl_->yield(timeout_ms);
}

bool MqttClient::isConnected() const {
    return pimpl_->connected;
}