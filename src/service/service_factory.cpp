//
// Created by wave on 2026/1/23.
//

#include <algorithm>

#include "logger/logger.h"
#include "service/service_factory.h"


namespace swan {
namespace services {

ServiceFactory& ServiceFactory::instance() {
    static ServiceFactory instance;
    return instance;
}

ServiceFactory::ServiceFactory() {
    LOG_DEBUG("ServiceFactory initialized");
}

std::shared_ptr<BaseService> ServiceFactory::createService(const std::string& service_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = creators_.find(service_type);
    if (it == creators_.end()) {
        LOG_ERROR("Service type not registered: {}", service_type);
        return nullptr;
    }

    try {
        auto service = it->second();
        LOG_DEBUG("Created service: {}", service_type);
        return service;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create service {}: {}", service_type, e.what());
        return nullptr;
    }
}

std::vector<std::shared_ptr<BaseService>> ServiceFactory::createAllServices() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<BaseService>> services;
    services.reserve(creators_.size());

    for (const auto& [service_type, creator] : creators_) {
        try {
            auto service = creator();
            services.push_back(service);
            LOG_DEBUG("Created service: {}", service_type);

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create service {}: {}", service_type, e.what());
        }
    }

    return services;
}

std::vector<std::string> ServiceFactory::getSupportedServiceTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> types;
    types.reserve(creators_.size());

    for (const auto& [service_type, _] : creators_) {
        types.push_back(service_type);
    }

    std::sort(types.begin(), types.end());
    return types;
}

std::shared_ptr<BaseService> ServiceFactory::discoverServiceByAction(
    const std::string& action_type) const {

    // 这里我们可以实现更复杂的发现逻辑
    // 暂时使用简单的类型映射
    std::lock_guard<std::mutex> lock(mutex_);

    // 尝试直接匹配
    auto it = creators_.find(action_type);
    if (it != creators_.end()) {
        try {
            return it->second();
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create service for action {}: {}",
                     action_type, e.what());
            return nullptr;
        }
    }

    // 如果没有直接匹配，尝试遍历所有服务
    for (const auto& [service_type, creator] : creators_) {
        auto service = creator();
        if (service->getSupportedActionType() == action_type) {
            LOG_DEBUG("Discovered service {} for action {}",
                     service_type, action_type);
            return service;
        }
    }

    LOG_WARN("No service found for action type: {}", action_type);
    return nullptr;
}

    bool ServiceFactory::initializeAllServices(const std::vector<std::shared_ptr<BaseService>>& services) {
    bool all_success = true;

    for (const auto& service : services) {
        auto result = service->initialize();
        if (!result) {
            LOG_ERROR("Failed to initialize service {}: {}",
                     service->getName(), result);
            all_success = false;
        }
    }

    return all_success;
}
} // namespace services
} // namespace swan