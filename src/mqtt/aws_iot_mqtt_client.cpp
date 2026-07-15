// src/mqtt/aws_iot_mqtt_client.cpp
#include "mqtt/aws_iot_mqtt_client.h"
#include "logger/logger.h" // 假设你的日志库

// 引入 AWS IoT Device SDK for C++ 的实际头文件
// 你需要根据你的 AWS SDK 安装路径和版本进行调整
#include <aws/crt/Api.h>
#include <aws/crt/StlAllocator.h>
#include <aws/crt/io/EventLoopGroup.h>
#include <aws/crt/io/HostResolver.h>
#include <aws/crt/io/ClientBootstrap.h>
#include <aws/crt/mqtt/MqttClient.h>
#include <aws/crt/mqtt/MqttConnection.h>
#include <aws/crt/auth/Credentials.h> // 如果需要 SigV4 认证

// 静态初始化 AWS CRT
static struct AwsCrtInit {
    AwsCrtInit() {
        Aws::Crt::ApiHandle::InitStaticState();
    }
    ~AwsCrtInit() {
        Aws::Crt::ApiHandle::CleanUpStaticState();
    }
} s_aws_crt_init;


AwsIotMqttClient::AwsIotMqttClient(const std::string& endpoint,
                                   const std::string& client_id,
                                   const std::string& root_ca_path,
                                   const std::string& cert_path,
                                   const std::string& private_key_path)
    : m_endpoint(endpoint),
      m_client_id(client_id),
      m_root_ca_path(root_ca_path),
      m_cert_path(cert_path),
      m_private_key_path(private_key_path)
{
    LOG_INFO("AwsIotMqttClient created for endpoint: {}", m_endpoint.c_str());
}

AwsIotMqttClient::~AwsIotMqttClient() {
    disconnect();
    LOG_INFO("AwsIotMqttClient destroyed.");
}

bool AwsIotMqttClient::connect() {
    if (m_connected) {
        LOG_WARN("AwsIotMqttClient already connected.");
        return true;
    }

    LOG_INFO("Connecting to AWS IoT Core endpoint: {} with client ID: {}", m_endpoint.c_str(), m_client_id.c_str());

    // 1. 创建 EventLoopGroup
    m_event_loop_group = std::make_unique<Aws::Crt::Io::EventLoopGroup>(1); // 1 个事件循环线程
    if (!m_event_loop_group) {
        LOG_ERROR("Failed to create EventLoopGroup.");
        return false;
    }

    // 2. 创建 HostResolver
    m_host_resolver = std::make_unique<Aws::Crt::Io::HostResolver>(*m_event_loop_group, 8, 1);
    if (!m_host_resolver) {
        LOG_ERROR("Failed to create HostResolver.");
        return false;
    }

    // 3. 创建 ClientBootstrap
    m_client_bootstrap = std::make_unique<Aws::Crt::Io::ClientBootstrap>(*m_event_loop_group, *m_host_resolver);
    if (!m_client_bootstrap) {
        LOG_ERROR("Failed to create ClientBootstrap.");
        return false;
    }

    // 4. 创建 MqttClient
    m_aws_mqtt_client = std::make_unique<Aws::Crt::Mqtt::MqttClient>(*m_client_bootstrap);
    if (!m_aws_mqtt_client) {
        LOG_ERROR("Failed to create MqttClient.");
        return false;
    }

    // 5. 配置 TLS
    Aws::Crt::Io::TlsContextOptions tlsCtxOptions = Aws::Crt::Io::TlsContextOptions::InitClientWithMtls(
        m_cert_path.c_str(), m_private_key_path.c_str()
    );
    tlsCtxOptions.OverrideDefaultTrustStoreFromPath(nullptr, m_root_ca_path.c_str());

    Aws::Crt::Io::TlsContext tlsContext(tlsCtxOptions);
    if (!tlsContext.Is  Valid()) {
        LOG_ERROR("Failed to create TlsContext: {}", Aws::Crt::ErrorDebugString(tlsContext.LastError()));
        return false;
    }

    // 6. 创建 MqttConnection
    Aws::Crt::Mqtt::MqttConnectionConfig connectionConfig(m_client_id.c_str(), m_endpoint.c_str(), 8883);
    connectionConfig.WithTlsContext(tlsContext);
    connectionConfig.WithCleanSession(true);
    connectionConfig.WithKeepAliveTimeout(60); // Keep-alive 60秒

    m_connection = m_aws_mqtt_client->NewConnection(connectionConfig);
    if (!m_connection) {
        LOG_ERROR("Failed to create MqttConnection: {}", Aws::Crt::ErrorDebugString(m_aws_mqtt_client->LastError()));
        return false;
    }

    // 7. 设置连接回调
    m_connection->OnConnectionCompleted = [&](
        Aws::Crt::Mqtt::MqttConnection& connection,
        int errorCode,
        Aws::Crt::Mqtt::ReturnCode returnCode,
        bool sessionPresent) {
        if (!errorCode) {
            m_connected = true;
            LOG_INFO("AWS IoT Core Connected! SessionPresent: {}", sessionPresent);
            if (m_status_callback) {
                m_status_callback(true, "Connected to AWS IoT Core");
            }
        } else {
            m_connected = false;
            LOG_ERROR("AWS IoT Core Connection failed: {} ({})", Aws::Crt::ErrorDebugString(errorCode), (int)returnCode);
            if (m_status_callback) {
                m_status_callback(false, Aws::Crt::ErrorDebugString(errorCode));
            }
        }
    };

    m_connection->OnDisconnect = [&](
        Aws::Crt::Mqtt::MqttConnection& connection) {
        m_connected = false;
        LOG_INFO("AWS IoT Core Disconnected.");
        if (m_status_callback) {
            m_status_callback(false, "Disconnected from AWS IoT Core");
        }
    };

    m_connection->OnConnectionInterrupted = [&](
        Aws::Crt::Mqtt::MqttConnection& connection,
        int error_code) {
        m_connected = false;
        LOG_WARN("AWS IoT Core Connection interrupted: {}", Aws::Crt::ErrorDebugString(error_code));
        if (m_status_callback) {
            m_status_callback(false, Aws::Crt::ErrorDebugString(error_code));
        }
    };

    m_connection->OnConnectionResumed = [&](
        Aws::Crt::Mqtt::MqttConnection& connection,
        int error_code,
        Aws::Crt::Mqtt::ReturnCode return_code,
        bool session_present) {
        if (!error_code) {
            m_connected = true;
            LOG_INFO("AWS IoT Core Connection resumed! SessionPresent: {}", session_present);
            if (m_status_callback) {
                m_status_callback(true, "Connection resumed to AWS IoT Core");
            }
        } else {
            m_connected = false;
            LOG_ERROR("AWS IoT Core Connection resume failed: {} ({})", Aws::Crt::ErrorDebugString(error_code), (int)return_code);
            if (m_status_callback) {
                m_status_callback(false, Aws::Crt::ErrorDebugString(error_code));
            }
        }
    };

    // 8. 设置消息回调
    m_connection->OnMessageReceived = [&](
        Aws::Crt::Mqtt::MqttConnection& connection,
        const Aws::Crt::String& topic,
        const Aws::Crt::ByteBuf& payload,
        bool dup,
        Aws::Crt::Mqtt::QOS qos,
        bool retain) {
        if (m_msg_callback) {
            std::string topic_str(topic.c_str());
            std::string payload_str(reinterpret_cast<const char*>(payload.buffer), payload.len);
            LOG_INFO("AWS IoT Core Message received on topic: {}, payload size: {}", topic_str.c_str(), payload_str.length());
            m_msg_callback(topic_str, payload_str);
        }
    };

    // 9. 连接
    auto connectionFuture = m_connection->Connect();
    connectionFuture.wait(); // 阻塞等待连接结果

    if (connectionFuture.get()) {
        LOG_INFO("Successfully initiated AWS IoT Core connection.");
        return true;
    } else {
        LOG_ERROR("Failed to connect to AWS IoT Core: {}", Aws::Crt::ErrorDebugString(m_connection->LastError()));
        m_connected = false;
        return false;
    }
}

void AwsIotMqttClient::disconnect() {
    if (m_connection && m_connected) {
        LOG_INFO("Disconnecting from AWS IoT Core.");
        auto disconnectFuture = m_connection->Disconnect();
        disconnectFuture.wait(); // 阻塞等待断开结果
        if (disconnectFuture.get()) {
            LOG_INFO("Successfully disconnected from AWS IoT Core.");
        } else {
            LOG_ERROR("Failed to disconnect from AWS IoT Core: {}", Aws::Crt::ErrorDebugString(m_connection->LastError()));
        }
        m_connected = false;
    }
}

bool AwsIotMqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
    if (!m_connected || !m_connection) {
        LOG_ERROR("Publish failed: not connected to AWS IoT Core.");
        return false;
    }

    Aws::Crt::Mqtt::QOS aws_qos = static_cast<Aws::Crt::Mqtt::QOS>(qos);
    Aws::Crt::ByteBuf payload_buf = Aws::Crt::ByteBufFromArray(reinterpret_cast<const uint8_t*>(payload.data()), payload.length());

    auto publishFuture = m_connection->Publish(
        topic.c_str(),
        aws_qos,
        retained,
        payload_buf
    );

    // 阻塞等待发布结果
    publishFuture.wait();
    if (publishFuture.get()) {
        LOG_INFO("Published message to AWS IoT Core topic: {} (QoS: {})", topic.c_str(), qos);
        return true;
    } else {
        LOG_ERROR("Failed to publish message to AWS IoT Core topic: {}: {}", topic.c_str(), Aws::Crt::ErrorDebugString(m_connection->LastError()));
        return false;
    }
}

bool AwsIotMqttClient::subscribe(const std::string& topic, int qos) {
    if (!m_connected || !m_connection) {
        LOG_ERROR("Subscribe failed: not connected to AWS IoT Core.");
        return false;
    }

    Aws::Crt::Mqtt::QOS aws_qos = static_cast<Aws::Crt::Mqtt::QOS>(qos);
    auto subscribeFuture = m_connection->Subscribe(
        topic.c_str(),
        aws_qos,
        [&](Aws::Crt::Mqtt::MqttConnection& connection,
            const Aws::Crt::String& topic,
            const Aws::Crt::ByteBuf& payload,
            bool dup,
            Aws::Crt::Mqtt::QOS qos,
            bool retain) {
            // 消息接收回调已在 connect() 中设置，这里只是为了满足 Subscribe API 的要求
            // 实际消息处理会通过 OnMessageReceived 回调
            (void)connection; (void)topic; (void)payload; (void)dup; (void)qos; (void)retain;
        }
    );

    // 阻塞等待订阅结果
    subscribeFuture.wait();
    if (subscribeFuture.get()) {
        LOG_INFO("Subscribed to AWS IoT Core topic: {} (QoS: {})", topic.c_str(), qos);
        return true;
    } else {
        LOG_ERROR("Failed to subscribe to AWS IoT Core topic: {}: {}", topic.c_str(), Aws::Crt::ErrorDebugString(m_connection->LastError()));
        return false;
    }
}

bool AwsIotMqttClient::unsubscribe(const std::string& topic) {
    if (!m_connected || !m_connection) {
        LOG_ERROR("Unsubscribe failed: not connected to AWS IoT Core.");
        return false;
    }

    auto unsubscribeFuture = m_connection->Unsubscribe(topic.c_str());
    unsubscribeFuture.wait(); // 阻塞等待取消订阅结果

    if (unsubscribeFuture.get()) {
        LOG_INFO("Unsubscribed from AWS IoT Core topic: {}", topic.c_str());
        return true;
    } else {
        LOG_ERROR("Failed to unsubscribe from AWS IoT Core topic: {}: {}", topic.c_str(), Aws::Crt::ErrorDebugString(m_connection->LastError()));
        return false;
    }
}

void AwsIotMqttClient::yield(int timeout_ms) {
    // AWS IoT Device SDK for C++ 通常不需要显式调用 yield
    // 它的事件循环在内部处理网络I/O和回调
    // 如果需要，可以在这里添加一个短暂的休眠，或者检查连接状态
    (void)timeout_ms; // 避免未使用参数警告
    // std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
}

bool AwsIotMqttClient::isConnected() const {
    return m_connected;
}

void AwsIotMqttClient::setMessageCallback(MqttMessageCallback cb) {
    m_msg_callback = std::move(cb);
}

void AwsIotMqttClient::setStatusCallback(MqttStatusCallback cb) {
    m_status_callback = std::move(cb);
}

// 内部回调函数实现 (如果需要，可以从 lambda 表达式中提取出来)
// void AwsIotMqttClient::on_connection_interrupted(Aws::Crt::Mqtt::MqttConnection& connection, int error_code) {
//     m_connected = false;
//     LOG_WARN("AWS IoT Core Connection interrupted: {}", Aws::Crt::ErrorDebugString(error_code));
//     if (m_status_callback) {
//         m_status_callback(false, Aws::Crt::ErrorDebugString(error_code));
//     }
// }

// void AwsIotMqttClient::on_connection_resumed(Aws::Crt::Mqtt::MqttConnection& connection, int error_code) {
//     if (!error_code) {
//         m_connected = true;
//         LOG_INFO("AWS IoT Core Connection resumed!");
//         if (m_status_callback) {
//             m_status_callback(true, "Connection resumed to AWS IoT Core");
//         }
//     } else {
//         m_connected = false;
//         LOG_ERROR("AWS IoT Core Connection resume failed: {}", Aws::Crt::ErrorDebugString(error_code));
//         if (m_status_callback) {
//             m_status_callback(false, Aws::Crt::ErrorDebugString(error_code));
//         }
//     }
// }

// void AwsIotMqttClient::on_message_received(Aws::Crt::Mqtt::MqttConnection& connection,
//                                          const Aws::Crt::String& topic,
//                                          const Aws::Crt::ByteBuf& payload,
//                                          bool dup,
//                                          Aws::Crt::Mqtt::QOS qos,
//                                          bool retain) {
//     if (m_msg_callback) {
//         std::string topic_str(topic.c_str());
//         std::string payload_str(reinterpret_cast<const char*>(payload.buffer), payload.len);
//         LOG_INFO("AWS IoT Core Message received on topic: {}, payload size: {}", topic_str.c_str(), payload_str.length());
//         m_msg_callback(topic_str, payload_str);
//     }
// }