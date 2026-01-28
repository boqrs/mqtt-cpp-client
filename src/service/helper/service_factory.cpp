//
// Created by wave on 2026/1/23.
//

#include <algorithm>

#include "logger/logger.h"
#include "../../../include/service/base/service_factory.h"


namespace swan {
namespace services {
    ServiceFactory& ServiceFactory::instance() {
    static ServiceFactory instance;
    return instance;
}
    swan::services::ServiceFactory& force_service_factory_init = swan::services::ServiceFactory::instance();

ServiceFactory::ServiceFactory() {
    LOG_DEBUG("ServiceFactory initialized");
}

    std::shared_ptr<BaseService> ServiceFactory::createService(const std::string& service_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 第一步：先查实例缓存 → 有则直接返回
    auto inst_it = service_instances_.find(service_type);
    if (inst_it != service_instances_.end()) {
        LOG_DEBUG("Reusing cached service instance: {}", service_type);
        return inst_it->second;
    }

    // 第二步：缓存未命中 → 执行创建函数新建实例
    auto creator_it = creators_.find(service_type);
    if (creator_it == creators_.end()) {
        LOG_ERROR("No creator found for service type: {}", service_type);
        return nullptr;
    }

    try {
        auto service = creator_it->second(); // 首次创建实例（触发构造函数）
        if (service) {
            service_instances_[service_type] = service; // 存入缓存
            LOG_DEBUG("Created and cached service instance: {}", service_type);
        }
        return service;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create service {}: {}", service_type, e.what());
        return nullptr;
    }
}


    std::vector<std::shared_ptr<BaseService>> ServiceFactory::createAllServices() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<BaseService>> services;

    for (const auto& [type, creator] : creators_) {
        // 优先从缓存取，无则创建
        auto inst_it = service_instances_.find(type);
        if (inst_it != service_instances_.end()) {
            services.push_back(inst_it->second);
            continue;
        }

        try {
            auto service = creator();
            if (service) {
                service_instances_[type] = service; // 存入缓存
                services.push_back(service);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create service {}: {}", type, e.what());
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

    std::lock_guard<std::mutex> lock(mutex_);

    // 第一步：遍历实例缓存，匹配action_type（优先复用已创建的实例）
    for (const auto& [service_type, service] : service_instances_) {
        if (service && service->getSupportedActionType() == action_type) {
            LOG_DEBUG("Discovered cached service {} for action {}", service_type, action_type);
            return service;
        }
    }

    // 第二步：缓存未命中 → 遍历创建函数，创建并缓存
    for (const auto& [service_type, creator] : creators_) {
        auto service = creator(); // 首次创建
        if (service && service->getSupportedActionType() == action_type) {
            service_instances_[action_type] = service; // 按action_type缓存（或按service_type）
            LOG_DEBUG("Discovered, created and cached service {} for action {}", service_type, action_type);
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