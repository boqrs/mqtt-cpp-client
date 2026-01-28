//
// Created by wave on 2026/1/23.
//

#pragma once

#include "protocol.pb.h"

#include "service/base_service.h"

namespace swan {
    namespace services {

        class LightService : public BaseService {
        public:
            LightService();
            ~LightService() override = default;

            // 覆盖基类方法
            std::vector<std::string> getSupportedCommands() const override;

            // 命令验证
            common::Result validateCommand(
                const swan::protocol::ControlCommand& cmd) const override;

        protected:
            // 执行具体命令
            common::Result doExecute(
                const swan::protocol::ControlCommand& cmd,
                const std::shared_ptr<command::CommandContext>& context) override;

        private:
            // 灯光状态
            std::atomic<bool> light_on_{false};

            // 实际控制硬件的方法
            common::Result turnOnLight();
            common::Result turnOffLight();
            common::Result getLightStatus();
        };

    } // namespace services
} // namespace swan