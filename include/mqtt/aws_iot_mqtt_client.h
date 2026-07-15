// include/mqtt/aws_iot_mqtt_client.h
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <atomic>

#include "i_mqtt_client.h"
// 引入 AWS CRT SDK 的核心头文件，提供完整类型定义
#include <aws/crt/Types.h> // For Aws::Crt::String, Aws::Crt::ByteBuf
#include <aws/crt/io/EventLoopGroup.h>
#include <aws/crt/io/HostResolver.h>
#include <aws/crt/io/ClientBootstrap.h>
#include <aws/crt/mqtt/MqttClient.h>
#include <aws/crt/mqtt/MqttConnection.h> // For Aws::Crt::Mqtt::QOS and MqttConnection
 

// 引入 AWS IoT Device SDK for C++ 的相关头文件
// 注意：这里使用占位符，实际项目中需要根据你安装的 AWS SDK 版本和路径进行调整
// 例如：
// #include <aws/crt/Api.h>
// #include <aws/crt/StlAllocator.h>
// #include <aws/crt/io/EventLoopGroup.h>
// #include <aws/crt/io/HostResolver.h>
// #include <aws/crt/io/ClientBootstrap.h>
// #include <aws/iot/MqttClient.h>
// #include <aws/iot/iot_shadow_client.h> // 如果需要 Shadow 服务

// 假设 AWS SDK 的命名空间和核心类
namespace Aws {
    namespace Crt {
        namespace Io {
            class EventLoopGroup;
            class HostResolver;
            class ClientBootstrap;
        }
        namespace Mqtt {
            class MqttClient;
            class MqttConnection;
        }
    }
}


class AwsIotMqttClient : public IMqttClient {
public:
    // 构造函数，接收 AWS IoT Core 连接所需的参数
    AwsIotMqttClient(const std::string& endpoint,
                     const std::string& client_id,
                     const std::string& root_ca_path,
                     const std::string& cert_path,
                     const std::string& private_key_path);
    ~AwsIotMqttClient() override;

    // 实现 IMqttClient 接口的所有纯虚函数
    bool connect() override;
    void disconnect() override;
    bool publish(const std::string& topic, const std::string& payload, int qos = 0, bool retained = false) override;
    bool subscribe(const std::string& topic, int qos = 0) override;
    bool unsubscribe(const std::string& topic) override;
    void yield(int timeout_ms = 100) override; // AWS SDK 可能内部处理，或提供类似机制
    bool isConnected() const override;
    void setMessageCallback(MqttMessageCallback cb) override;
    void setStatusCallback(MqttStatusCallback cb) override;

private:
    // AWS IoT Core 连接参数
    std::string m_endpoint;
    std::string m_client_id;
    std::string m_root_ca_path;
    std::string m_cert_path;
    std::string m_private_key_path;

    // AWS IoT Device SDK for C++ 的内部客户端对象
    // 注意：这里使用占位符，实际需要根据 AWS SDK 的具体类型进行替换
    std::unique_ptr<Aws::Crt::Io::EventLoopGroup> m_event_loop_group;
    std::unique_ptr<Aws::Crt::Io::HostResolver> m_host_resolver;
    std::unique_ptr<Aws::Crt::Io::ClientBootstrap> m_client_bootstrap;
    std::unique_ptr<Aws::Crt::Mqtt::MqttClient> m_aws_mqtt_client;
    std::shared_ptr<Aws::Crt::Mqtt::MqttConnection> m_connection; // 使用 shared_ptr 因为回调可能持有

    std::atomic<bool> m_connected = false;
    MqttMessageCallback m_msg_callback;
    MqttStatusCallback m_status_callback;

    // 内部回调函数，将 AWS SDK 的回调桥接到 IMqttClient 的回调
    void on_connection_interrupted(Aws::Crt::Mqtt::MqttConnection& connection, int error_code);
    void on_connection_resumed(Aws::Crt::Mqtt::MqttConnection& connection, int error_code);
    void on_message_received(Aws::Crt::Mqtt::MqttConnection& connection,
                             const Aws::Crt::String& topic,
                             const Aws::Crt::ByteBuf& payload,
                             bool dup,
                             Aws::Crt::Mqtt::QOS qos,
                             bool retain);
};