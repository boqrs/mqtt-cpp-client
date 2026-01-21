//
// Created by wave on 2026/1/21.
//

#pragma once
#include "command/dispatcher.h"
#include "command/handler/light_control.h"
#include "command/handler/temperature_control.h"
#include "command/handler/job_control.h"
#include "service/light_service.h"
#include "service/temperature_service.h"
#include "service/job_service.h"
#include <iostream>

namespace swan {

    /**
     * @brief 初始化助手，负责创建和注册所有命令处理器
     * 这是一个纯头文件的辅助类，不需要.cpp文件
     */
    class Initializer {
    public:
        /**
         * @brief 初始化所有命令处理器
         * @param dispatcher 命令分发器
         */
        static void initializeHandlers(command::CommandDispatcher& dispatcher) {
            // 创建业务服务实例
            static auto lightService = std::make_shared<service::LightService>();
            static auto tempService = std::make_shared<service::TemperatureService>();
            static auto jobService = std::make_shared<service::JobService>();

            // 创建并注册处理器
            dispatcher.registerHandler("light_control",
                std::make_shared<command::LightControlHandler>(lightService));

            dispatcher.registerHandler("temperature_control",
                std::make_shared<command::TemperatureControlHandler>(tempService));

            dispatcher.registerHandler("new_job",
                std::make_shared<command::NewJobHandler>(jobService));

            // 可以在这里注册更多处理器...

            // 打印已注册的命令
            auto actions = dispatcher.getSupportedActions();
            std::cout << "Registered command handlers: ";
            for (size_t i = 0; i < actions.size(); ++i) {
                std::cout << actions[i];
                if (i < actions.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
        }
    };

} // namespace swan