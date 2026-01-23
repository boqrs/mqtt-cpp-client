//
// Created by wave on 2026/1/23.
//

// services/unregister_service.h
#pragma once

#include "service/base_service.h"
#include "protocol.pb.h"

namespace swan {
    namespace services {

        class UnregisterService : public BaseService {
        public:
            UnregisterService();
            ~UnregisterService() override = default;

            std::vector<std::string> getSupportedCommands() const override;

            common::Result validateCommand(
                const swan::device::ControlCommand& cmd) const override;

        protected:
            common::Result doExecute(
                const swan::device::ControlCommand& cmd,
                const std::shared_ptr<protocol::command::CommandContext>& context) override;

        private:
            // 注销步骤
            common::Result backupDeviceData();
            common::Result clearUserData();
            common::Result resetNetworkConfig();
            common::Result shutdownServices();

            // 进度回调
            void updateProgress(int progress, const std::string& message);
        };

    } // namespace services
} // namespace swan