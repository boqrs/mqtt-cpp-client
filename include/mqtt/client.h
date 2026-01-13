//
// Created by wave on 2026/1/8.
//
#pragma  once

#include <iostream>
#include <string>
#include <functional>
#include <memory>

using  messageCallback = std::function<void(const std::string& topic, const std::string& payload)>;

struct Impl{
    int sockfd = -1;
    std::string broker_ip;
    int broker_port=1883;
    std::string client_id;

    static  constexpr  size_t  SEND_BUF_SIZE=1024;
    static  constexpr size_t   RECV_BUF_SIZE=1024;
    unsigned char send_buf[SEND_BUF_SIZE];
    unsigned char recv_buf[RECV_BUF_SIZE];

    messageCallback user_cb;

    bool  connected= false;
    bool should_stop = false;

    Impl(const std::string& ip, int port, const std::string& id);

    ~Impl();
    void disconnect_internal();

    bool resolve_host(struct sockaddr_in& addr);

    bool connect(const std::string& username = "",
                 const std::string& password = "");

    bool publish(const std::string& topic,
                 const std::string& message,
                 int qos = 0);

    bool subscribe(const std::string& topic, int qos = 0);

    void yield(int timeout_ms = 100);

    void sendPubAck(unsigned short packet_id);

    void handleSubAckPacket(unsigned char* buf, int buflen);

    void handlePublishPacketUsingPaho(unsigned char* buf, int buflen);

};

class MqttClient {
public:
    MqttClient(const std::string& ip, int port, const std::string& client_id);
    ~MqttClient();

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;

    MqttClient(MqttClient&&) noexcept;
    MqttClient& operator=(MqttClient&&) noexcept;

    bool connect(const std::string& username, const std::string& password);
    void  disconnect();
    bool  publish(const std::string& topic, const std::string& payload, int qos);
    bool subscribe(const std::string& topic, int qos);
    void setMessageCallback(messageCallback cb);
    void yield(int timeout_ms = 100);
    bool isConnected()const;

private:
   std::unique_ptr<Impl> pimpl_;
};



