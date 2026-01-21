## 项目概述
是一个基于 C++ 开发的设备状态监控与发布系统，专门用于工业设备的实时状态监控和数据传输。系统通过 MQTT 协议将设备状态发布到云端，支持多种发布模式和实时事件通知。

## 功能特性
### 核心功能
- **多模式发布**：支持心跳模式、阈值模式和混合模式，适应不同网络环境。
- **实时状态监控**：监控设备温度、打印进度、材料状态等关键指标。
- **事件驱动**：支持状态变更事件、发布成功/失败事件等。
- **可扩展的监听器**：允许注册多个监听器，实时接收状态变更通知。
- **Protocol Buffers序列化**：使用Google Protocol Buffers进行高效的数据序列化。
- **跨平台**：基于标准C++14，支持Linux、Windows和macOS。

## 系统架构


┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│ 设备状态生成 │ │ MQTT发布器 │ │ MQTT代理 │
│ │───>│ │───>│ (Broker) │
│ (模拟/真实) │ │ │ │ │
└─────────────────┘ └─────────────────┘ └─────────────────┘
│ │ │
│ │ │
▼ ▼ ▼
┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
│ 状态监听器 │ │ 数据处理器 │ │ 状态订阅者 │
│ (Listener) │ │ (Processor) │ │ (Subscriber) │
└─────────────────┘ └─────────────────┘ └─────────────────┘

## 项目结构

```text
project/
├── CMakeLists.txt # 项目构建配置
├── main.cpp # 程序入口,使用 demo
├── proto/
│ └── device_status.proto # Protobuf消息定义
├── include/
│ ├── mqtt/
│ │ ├── publisher.h
│ │ ├── client.h
│ │ ├── impl.h
│ │ └── thread.h
│ ├── processor/
│ │ ├── data_validator.h
│ │ └── data_transformer.h
│ └── utils/
│ └── models/
│ └── protocol.h
├── src/
│ ├── mqtt/
│ │ ├── impl.cpp
│ │ ├── client.cpp
│ │ ├── thread.cpp
│ │ └── publisher.cpp
│ ├── processor/
│ │ ├── data_validator.cpp
│ │ └── data_transformer.cpp
│ └── utils/
│ └── models/
└── docker/
└── Dockerfile # Docker容器化配置

```

## 协议结构
       系统中的协议分为两种，一种是上行协议(设备端-->服务端)，另一种是下行协议(服务端到-->设备端)。考虑到集群
    的资源限制本着客户端尽量为服务端的性能考虑的原则，设备和 MQTT 集群的交互只是用一个 topic，这样尽量减少设备数量
    对应的 topic 数量限制。MQTT 本身支持消息抑制功能，也就是客户端发送的消息可以不让本客户端收到这样设备向 topic 发送的
    状态数据一定不会回环到设备本身，toppic 的基本规则如下:
    形如: swan/device/{modename}/{sn}/state-cmd
    其中modename是设备的信号，sn 为设备的序列号
   
   1. 协议设计:
      协议层面为了兼顾上下行消息，总体的协议遵循如下标准:
      ```json
      {
        "messageType": "device_state",
        "payload": {
         "actionType": "device_status"  
      }
      }
      ```
      为了方便阅读我们以json的形式描述，如上其中messageType其实表述的是消息的类型，上行消息为device_state
      actionType表示子协议名称。下行消息的messageType是device_cmd actionType是具体的命令名称

   