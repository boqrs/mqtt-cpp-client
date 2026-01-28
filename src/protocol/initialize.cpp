//
// Created by wave on 2026/1/27.

#include "protocol/initialize.h"

#include <config.h>

#include "logger/logger.h"
#include "../../include/service/base/service_factory.h"
#include "../../include/service/base/base_service.h"
#include "utils/result.h" // 仅内部业务逻辑使用，不对外返回

namespace swan {
namespace init {

namespace {
    std::shared_ptr<proparser::ProtocolParser> g_parser;
    std::shared_ptr<prodispatcher::CommandDispatcher> g_dispatcher;
    bool g_is_initialized = false;
    std::mutex g_init_mutex;
    std::vector<std::shared_ptr<services::BaseService>> g_initialized_services;
}

bool initCommandProcessing(bool parser_strict_mode, size_t dispatcher_max_queue_size) {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (g_is_initialized) {
        LOG_WARN("Command processing already initialized");
        return true;
    }

    LOG_INFO("Starting command processing initialization...");

    try {
        g_parser = std::make_shared<proparser::ProtocolParser>();
        g_parser->setStrictMode(parser_strict_mode);
        g_parser->setProtocolVersion({1, 0, 0});
        LOG_INFO("ProtocolParser initialized (strict mode: {})", parser_strict_mode);
    } catch (const std::exception& e) {
        LOG_ERROR("Step 1 Failed to initialize ProtocolParser:: {}", e.what());
        return false;
    }

    // ========== 步骤2：初始化命令分发器 ==========
    try {
        g_dispatcher = std::make_shared<prodispatcher::CommandDispatcher>();
        g_dispatcher->setMaxQueueSize(dispatcher_max_queue_size);
        g_dispatcher->setEnableQueue(true); // 启用队列
        LOG_INFO("CommandDispatcher initialized (max queue size: {})", dispatcher_max_queue_size);
    } catch (const std::exception& e) {
        LOG_ERROR("Step 2 failed to initialize CommandDispatcher: {}", e.what());
        g_parser.reset(); // 回滚解析器
        return false;
    }

    // ========== 步骤3：初始化所有已注册的服务 ==========
    auto& factory = services::ServiceFactory::instance();
    auto supported_service_types = factory.getSupportedServiceTypes();

    if (supported_service_types.empty()) {
        LOG_ERROR("Step 3 No services registered in ServiceFactory");
        g_dispatcher.reset();
        g_parser.reset();
        return false;
    }

    LOG_INFO("Found {} registered services, initializing...", supported_service_types.size());
    bool all_services_init_success = true;
    std::string service_error_details;

    for (const auto& service_type : supported_service_types) {
        auto service = factory.createService(service_type);
        if (!service) {
            service_error_details += "Failed to create service: " + service_type + "; ";
            LOG_ERROR("Failed to create service: {}", service_type);
            all_services_init_success = false;
            continue;
        }

        // 初始化服务（调用BaseService::initialize）
        if (!service->initialize()) {
            service_error_details += "Failed to initialize service: " + service_type + "; ";
            LOG_ERROR("Failed to initialize service: {}", service_type);
            all_services_init_success = false;
            continue;
        }

        // 存储已初始化的服务（用于后续停止）
        g_initialized_services.push_back(service);
        LOG_INFO("Successfully initialized service: {}", service_type);
    }

    if (!all_services_init_success) {
        LOG_ERROR("Step 3 Some services failed to initialize: {}", service_error_details);
        // 回滚：停止已初始化的服务
        for (const auto& service : g_initialized_services) {
            service->shutdown();
        }
        g_initialized_services.clear();
        g_dispatcher.reset();
        g_parser.reset();
        return false;
    }

    // ========== 步骤4：将服务注册到分发器 ==========
    bool all_services_register_success = true;
    std::string register_error_details;

    for (const auto& service : g_initialized_services) {
        std::string action_type = service->getSupportedActionType();
        if (!g_dispatcher->registerService(service)) {
            register_error_details += "Failed to register service " + service->getName() + " for action: " + action_type + "; ";
            LOG_ERROR("Failed to register service {} for action: {}", service->getName(), action_type);
            all_services_register_success = false;
        } else {
            LOG_INFO("Registered service {} for action: {}", service->getName(), action_type);
        }
    }

    if (!all_services_register_success) {
        LOG_ERROR("Step 4 Some services failed to register to dispatcher:: {}", register_error_details);
        // 回滚：停止服务 + 清理分发器/解析器
        for (const auto& service : g_initialized_services) {
            service->shutdown();
        }
        g_initialized_services.clear();
        g_dispatcher.reset();
        g_parser.reset();
        return false;
    }

    // ========== 初始化完成 ==========
    g_is_initialized = true;
    LOG_INFO("Command processing initialized successfully ({} services loaded)",
             g_initialized_services.size());
    return true;
}

bool shutdownCommandProcessing() {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (!g_is_initialized) {
        LOG_WARN("Command processing not initialized, skip shutdown");
        return true; // 未初始化视为"成功"
    }

    LOG_INFO("Starting command processing shutdown...");
    bool all_shutdown_success = true;
    std::string shutdown_error_details;

    // ========== 步骤1：停止所有已初始化的服务 ==========
    for (const auto& service : g_initialized_services) {
        if (!service->shutdown()) {
            shutdown_error_details += "Failed to shutdown service: " + service->getName() + "; ";
            LOG_ERROR("Failed to shutdown service: {}", service->getName());
            all_shutdown_success = false;
        } else {
            LOG_INFO("Shutdown service: {}", service->getName());
        }
    }
    g_initialized_services.clear();

    // ========== 步骤2：清理分发器和解析器 ==========
    g_dispatcher.reset();
    g_parser.reset();

    // ========== 状态重置 ==========
    g_is_initialized = false;

    if (!all_shutdown_success) {
        LOG_ERROR("Shutdown Some services failed to shutdown: {}", shutdown_error_details);
        return false; // 部分服务停止失败，返回false
    }

    LOG_INFO("Command processing shutdown completed");
    return true;
}

std::shared_ptr<proparser::ProtocolParser> getProtocolParser() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    return g_parser;
}

std::shared_ptr<prodispatcher::CommandDispatcher> getCommandDispatcher() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    return g_dispatcher;
}

bool isCommandProcessingInitialized() {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    return g_is_initialized;
}

bool dispatchMqttCommand(const std::string& topic, const std::string& payload) {
    // 前置校验：框架必须已初始化（队列+线程已创建）
    LOG_INFO("dispatchMqttCommand rev commad from topic: {}", topic);
    if (!isCommandProcessingInitialized()) {
        LOG_ERROR("Command processing not initialized, cannot dispatch MQTT command");
        return false;
    }

    if (payload.empty()) {
        LOG_ERROR("MQTT payload is empty, topic: {}", topic);
        return false;
    }

    // 1. 获取框架核心实例（Parser+Dispatcher）
    auto parser = getProtocolParser();
    auto dispatcher = getCommandDispatcher();
    if (!parser || !dispatcher) {
        LOG_ERROR("Parser/Dispatcher instance is null");
        return false;
    }

    LOG_DEBUG("Start dispatching MQTT command: topic={}, payload_size={} bytes",
              topic, payload.size());

    // 2. 解析原始payload为UnifiedMessage（Protobuf反序列化）
    protocol::UnifiedMessage msg;
    if (!msg.ParseFromString(payload)) {
        LOG_ERROR("Failed to parse payload to UnifiedMessage, topic: {}", topic);
        return false;
    }

    // 3. 校验消息合法性（非空、是命令类型）
    if (!msg.has_payload() || !msg.payload().has_device_cmd()) {
        LOG_ERROR("Invalid command message: no payload/device_cmd, topic: {}", topic);
        return false;
    }
    if (msg.message_type() != "device_cmd") {
        LOG_WARN("Non-command message ignored: topic={}, msg_type={}",
                 topic, msg.message_type());
        return false;
    }

    std::string device_id = msg.payload().device_cmd().cmd().c_str();
    if (device_id.empty()) {
        device_id = DEVICE_SN; // 兜底用配置的设备SN
    }

    auto context = std::make_shared<command::CommandContext>(
        device_id,
        msg.request_id(),
        std::chrono::system_clock::now()
    );

    // 5. 设置响应回调（可选：发布执行结果到MQTT）
    context->setResponseCallback([topic, request_id = msg.request_id()](const common::Result& result) {
       /* std::string resp_topic = fmt::format("{}/result", topic);
        if (result.isSuccess()) {
            LOG_INFO("Command executed successfully (request_id={}), publish to: {}",
                     request_id, resp_topic);
        } else {
            LOG_ERROR("Command executed failed (request_id={}): {}, publish to: {}",
                      request_id, result.getMessage(), resp_topic);
        }

        // 构造响应消息并发布（贴合你的Protobuf结构）
        protocol::UnifiedMessage resp_msg;
        resp_msg.set_message_type("cmd_result");
        resp_msg.set_request_id(request_id);
        resp_msg.mutable_payload()->set_action_type("cmd_result");
        resp_msg.mutable_payload()->set_result_code(result.getCode());
        resp_msg.mutable_payload()->set_result_msg(result.getMessage());

        std::string resp_payload;
        if (resp_msg.SerializeToString(&resp_payload)) {
            MqttManager::getInstance().publish(resp_topic, resp_payload);
        }*/

       LOG_INFO("cmd response: {}", result.toString());
    });

    // 6. 分发命令到Dispatcher（自动入缓冲队列，独立线程执行）
    // Dispatcher内部逻辑：
    // - enable_queue=true时，命令进入queue_队列；
    // - queue_processor_thread独立线程从队列取命令执行；
    // - 执行策略：QUEUED（入队）/DIRECT（直接执行）由Dispatcher决定
    common::Result dispatch_result = dispatcher->dispatchUnifiedMessage(msg, context);
    if (!dispatch_result.isSuccess()) {
        LOG_ERROR("Failed to dispatch command (request_id={}): {}",
                  msg.request_id(), dispatch_result.toString());
        return false;
    }

    // 日志：命令成功入队
    LOG_INFO("Command enqueued successfully (request_id={}, queue_size={})",
             msg.request_id(), dispatcher->getQueueSize());
    return true;
}













} // namespace init
} // namespace swan