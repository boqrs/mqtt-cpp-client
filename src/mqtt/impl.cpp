//
// Created by wave on 2026/1/8.
//
#include "mqtt/client.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>


#include "MQTTPacket.h"
#include "logger/logger.h"

int transport_send(int sockfd, const unsigned char* buf, size_t len) {
    ssize_t sent = 0;
    while (sent < static_cast<ssize_t>(len)) {
        ssize_t rc = send(sockfd, buf + sent, len - sent, 0);
        if (rc <= 0) {
            if (rc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // 非阻塞socket，稍后重试
                continue;
            }
            return -1;
        }
        sent += rc;
    }
    return static_cast<int>(sent);
}

int transport_recv(int sockfd, unsigned char* buf, size_t buf_size, int timeout_ms) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int rc = select(sockfd + 1, &readfds, nullptr, nullptr, &tv);
    if (rc > 0 && FD_ISSET(sockfd, &readfds)) {
        rc = recv(sockfd, buf, buf_size, 0);
        return rc;
    }
    return (rc == 0) ? 0 : -1; // 0表示超时，-1表示错误
}

bool Impl::resolve_host(struct sockaddr_in &addr) {
    std::memset(&addr, 0, sizeof (addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(broker_port));

    if (inet_pton(AF_INET, broker_ip.c_str(), &addr.sin_addr) == 1){
        return true;
    }

    struct hostent*he = gethostbyname(broker_ip.c_str());
    if(he&&he->h_addrtype == AF_INET&&he->h_addr_list[0]){
        std::memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof (struct in_addr));
        return true;
    }

    LOG_ERROR("[MQTT] can not resolve host: ", broker_ip);
    return false;
}

bool Impl::connect(const std::string &username, const std::string &password) {
    if(connected){
        LOG_ERROR("[MQTT] server has been connected");
        return true;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd <0){
        LOG_ERROR("[MQTT] failed to create socket");
        return false;
    }

    struct sockaddr_in server_addr;
    if (!resolve_host(server_addr)){
        close(sockfd);
        sockfd = -1;
        return false;
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (::connect(sockfd, (struct  sockaddr*)&server_addr, sizeof (server_addr)) <0){
        LOG_ERROR("[MQTT] failed to connected to server {}, (errno: {})", strerror(errno), errno);
        close(sockfd);
        sockfd = -1;
        return false;
    }

    // 5. CONNECT packet
    MQTTPacket_connectData connect_data = MQTTPacket_connectData_initializer;
    connect_data.clientID.cstring = const_cast<char*>(client_id.c_str());
    connect_data.keepAliveInterval = 60;
    connect_data.cleansession = 1;

    if (!username.empty()) {
        MQTTString username_str = MQTTString_initializer;
        username_str.cstring = const_cast<char*>(username.c_str());
        connect_data.username = username_str;
        if (!password.empty()) {
            MQTTString password_str = MQTTString_initializer;
            password_str.cstring = const_cast<char*>(password.c_str());
            connect_data.password = password_str;
        }
    }

    // 6. send CONNECT packet
    int packet_len = MQTTSerialize_connect(
            send_buf, sizeof(send_buf),
            &connect_data
    );

    if (packet_len <= 0) {
        LOG_ERROR("[MQTT] failed to marsh connect packet");
        disconnect_internal();
        return false;
    }

    if (transport_send(sockfd, send_buf, packet_len) != packet_len) {
        LOG_ERROR("[MQTT] failed to send connect packet");
        disconnect_internal();
        return false;
    }

    // 7. rev CONNACK response（
    int rc = transport_recv(sockfd, recv_buf, sizeof(recv_buf), 5000);
    if (rc <= 0) {
        if (rc == 0){
            LOG_ERROR("[MQTT] Recv Connect Ack timeout");
        } else{
            LOG_ERROR("[MQTT] Recv Connect Ack failed");
        }
        disconnect_internal();
        return false;
    }

    // 8. parse CONNACK
    unsigned char session_present, connack_rc;
    if (MQTTDeserialize_connack(&session_present, &connack_rc,
                                recv_buf, rc) != 1) {
        LOG_ERROR("[MQTT] failed to parseCONNACK");
        disconnect_internal();
        return false;
    }

    if (connack_rc != 0) {
        LOG_ERROR("[MQTT] refused to connect, code: {}", static_cast<int>(connack_rc));
        disconnect_internal();
        return false;
    }

    connected = true;
    LOG_INFO("[MQTT] connect successfully");
    return true;

}

bool Impl::publish(const std::string &topic, const std::string &payload, int qos) {
    if (!connected) {
        LOG_ERROR("[MQTT] connected is empty");
        return false;
    }

    if (qos < 0) qos = 0;
    if (qos > 1) qos = 1;

    MQTTString topicString = MQTTString_initializer;
    topicString.cstring = const_cast<char*>(topic.c_str());

    int packet_len = MQTTSerialize_publish(
            send_buf, sizeof(send_buf),
            0,      // dup
            qos,    // qos
            0,      // retained
            0,      // packet_id (QoS 0不需要)
            topicString,
            reinterpret_cast<unsigned char*>(const_cast<char*>(payload.c_str())),
            static_cast<int>(payload.length())
    );


    if (packet_len <= 0) {
        LOG_ERROR("[MQTT] failed marsh publish packet");
        return false;
    }

    if (transport_send(sockfd, send_buf, packet_len) != packet_len) {
        LOG_ERROR("[MQTT] failed to send publish packet");
        return false;
    }

    return true;
}


bool Impl::subscribe(const std::string &topic, int qos) {
    if (!connected) {
        LOG_ERROR("[MQTT] connect is empty，failed to subscribe");
        return false;
    }

    static unsigned short packet_id = 1;


    MQTTString topicList[1];
    topicList[0] = MQTTString_initializer;
    topicList[0].cstring = const_cast<char*>(topic.c_str());
    int requestedQoS[1] = {qos};

    int packet_len = MQTTSerialize_subscribe(
            send_buf, sizeof(send_buf),
            0,      // dup
            packet_id++,
            1,      // count
            topicList,
            requestedQoS
    );

    if (packet_len <= 0) {
        LOG_ERROR("[MQTT] failed to marsh sub packet");
        return false;
    }

    if (transport_send(sockfd, send_buf, packet_len) != packet_len) {
        LOG_ERROR("[MQTT] failed to send sub packet");
        return false;
    }

    int rc = transport_recv(sockfd, recv_buf, sizeof(recv_buf), 2000);
    if (rc > 0) {
        LOG_ERROR("[MQTT] sub topic: ( Qos: {})", qos);
        return true;
    }
    return false;
}

void Impl::sendPubAck(unsigned short packet_id) {
    if (!connected) return;

    unsigned char puback_buf[4];
    puback_buf[0] = 0x40; // PUBACK 类型 (0x40 = 0100 0000)
    puback_buf[1] = 0x02; // 剩余长度 = 2
    puback_buf[2] = (packet_id >> 8) & 0xFF;
    puback_buf[3] = packet_id & 0xFF;

    if (transport_send(sockfd, puback_buf, 4) != 4) {
        LOG_ERROR("failed to send PUBACK");
    }
}

// 使用 Paho C 库解析 PUBLISH 报文
void Impl::handlePublishPacketUsingPaho(unsigned char* buf, int buflen) {
    if (!user_cb) return;

    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short packetid;
    int payloadlen_in;
    unsigned char* payload_in;
    MQTTString receivedTopic;

    // 使用 Paho C 库的解析函数
    int success = MQTTDeserialize_publish(
            &dup, &qos, &retained, &packetid,
            &receivedTopic, &payload_in, &payloadlen_in,
            buf, buflen
    );

    if (success != 1) {
        LOG_ERROR("[MQTT] failed to parse publish packet");
        return;
    }

    // 转换主题名
    std::string topic;
    if (receivedTopic.lenstring.len > 0) {
        topic = std::string(receivedTopic.lenstring.data,
                            receivedTopic.lenstring.len);
    } else if (receivedTopic.cstring) {
        topic = std::string(receivedTopic.cstring);
    } else {
        LOG_ERROR("[MQTT] unknown topic name");
        return;
    }

    std::string payload;
    if (payloadlen_in > 0 && payload_in) {
        payload = std::string(reinterpret_cast<char*>(payload_in), payloadlen_in);
    }

    LOG_ERROR("[MQTT] rev packet: topic: {}, qos: {}, dup: {}, RETAIN:{}, PID:{}, Size:{}",
              topic, qos, static_cast<int>(dup), static_cast<int>(retained), packetid, packetid, payloadlen_in);

    // 回调给用户
    user_cb(topic, payload);

    if (qos == 1) {
        sendPubAck(packetid);
    }
}

void Impl::handleSubAckPacket(unsigned char* buf, int buflen) {
    unsigned char packet_type = (buf[0] & 0xF0) >> 4;
    if (packet_type != 9) return;

    unsigned short packetid;
    int count;
    int granted_qos;

    int success = MQTTDeserialize_suback(&packetid, 1, &count, &granted_qos, buf, buflen);

    if (success == 1 && count == 1) {

        LOG_ERROR("[MQTT] sub ack: PacketID={}, QoS={}",
                  packetid, granted_qos);
    } else {
        LOG_ERROR("[MQTT] parse SUBACK failed" );
    }
}


void Impl::yield(int timeout_ms) {
    if (!connected || timeout_ms <= 0) return;

    int rc = transport_recv(sockfd, recv_buf,
                            sizeof(recv_buf), timeout_ms);
    if (rc > 0) {
        unsigned char packet_type = recv_buf[0] >> 4;

        switch (packet_type) {
            case 3:
                if (user_cb) {
                    LOG_ERROR("[MQTT] rev packet ({}bytes)", rc);
                    handlePublishPacketUsingPaho(recv_buf, rc);
                }
                break;
            case 9:
                handleSubAckPacket(recv_buf, rc);
                break;
            case 4:
                break;
            default:
                LOG_ERROR("[MQTT] unknown packet: {}", static_cast<int>(packet_type));
        }
    }
}



void Impl::disconnect_internal(){
    if (sockfd >=0){
        close(sockfd);
        sockfd = -1;
    }

    connected = false;
}

Impl::Impl(const std::string& ip, int port, const std::string& id):broker_ip(ip), broker_port(port), client_id(id){
    std::memset(send_buf, 0, sizeof(send_buf));
    std::memset(recv_buf, 0, sizeof(recv_buf));
}

Impl::~Impl(){
    disconnect_internal();
}