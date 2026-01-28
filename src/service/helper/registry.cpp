//
// Created by wave on 2026/1/27.
//

#include "../../../include/service/base/service_factory.h"
#include "service/light_service.h"
#include "service/cloud_job_service.h"
#include "service/profile_service.h"
#include "service/stream_service.h"
#include "service/temperature_service.h"
#include "service/unregister_service.h"
#include "logger/logger.h"

namespace swan {
    namespace services {

        // 显式注册所有服务（主动调用，绕过静态初始化坑）
        bool registerAllServices() {
            auto& factory = ServiceFactory::instance();
            bool all_success = true;

            // 注册LightService：服务类型"light_service"，对应action_type"light_control"
            if (!factory.registerService<LightService>("light_service")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }

            if (!factory.registerService<PrintService>("print_control")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }

            if (!factory.registerService<UserService>("user_profile")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }

            if (!factory.registerService<StreamService>("stream_control")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }

            if (!factory.registerService<TemperatureService>("temperature_control")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }

            if (!factory.registerService<UnregisterService>("device_unregister")) {
                LOG_ERROR("Failed to register LightService");
                all_success = false;
            }
            LOG_INFO("Explicit register services done, supported types count: {}",
                     factory.getSupportedServiceTypes().size());
            return all_success;
        }

    } // namespace services
} // namespace swan