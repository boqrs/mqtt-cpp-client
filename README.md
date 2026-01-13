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
│ │ ├── device_status_publisher.h
│ │ ├── client.h
│ │ ├── impl.h
│ │ └── thread.h
│ ├── processor/
│ │ ├── data_validator.h
│ │ └── data_transformer.h
│ └── utils/
│ └── models/
│ └── device_status.h
├── src/
│ ├── mqtt/
│ │ ├── impl.cpp
│ │ ├── client.cpp
│ │ ├── thread.cpp
│ │ └── device_status_publisher.cpp
│ ├── processor/
│ │ ├── data_validator.cpp
│ │ └── data_transformer.cpp
│ └── utils/
│ └── models/
│ └── device_status.cpp
└── docker/
└── Dockerfile # Docker容器化配置

```