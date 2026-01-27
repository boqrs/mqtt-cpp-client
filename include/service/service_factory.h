//
// Created by wave on 2026/1/23.
//

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include "service/base_service.h"

namespace swan {
namespace services {

class ServiceFactory {
public:
    using ServiceCreator = std::function<std::shared_ptr<BaseService>()>;

    // 单例访问
    static ServiceFactory& instance();

    // 禁止拷贝
    ServiceFactory(const ServiceFactory&) = delete;
    ServiceFactory& operator=(const ServiceFactory&) = delete;

    // 服务注册
    template<typename T>
    bool registerService(const std::string& service_type) {
        static_assert(std::is_base_of<BaseService, T>::value,
                     "T must inherit from BaseService");

        std::lock_guard<std::mutex> lock(mutex_);

        if (creators_.find(service_type) != creators_.end()) {
            return false;
        }

        creators_[service_type] = []() -> std::shared_ptr<BaseService> {
            return std::make_shared<T>();
        };

        return true;
    }

    // 服务创建
    std::shared_ptr<BaseService> createService(const std::string& service_type);

    // 批量创建所有服务
    std::vector<std::shared_ptr<BaseService>> createAllServices();

    // 获取支持的服务类型
    std::vector<std::string> getSupportedServiceTypes() const;

    // 服务发现
    std::shared_ptr<BaseService> discoverServiceByAction(
        const std::string& action_type) const;

    // 服务初始化
    bool initializeAllServices(const std::vector<std::shared_ptr<BaseService>>& services);

    // 自动注册辅助类
    template<typename T>
    struct AutoRegister {
        AutoRegister(const std::string& service_type) {
            ServiceFactory::instance().registerService<T>(service_type);
        }
    };
private:
    ServiceFactory();
    ~ServiceFactory() = default;

    std::unordered_map<std::string, ServiceCreator> creators_;
    mutable std::mutex mutex_;
};

} // namespace services
} // namespace swan
