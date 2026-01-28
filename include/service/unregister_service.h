//
// Created by wave on 2026/1/23.
//

#pragma once

#include "base/base_service.h"
#include "protocol.pb.h"

namespace swan {
    namespace services {

        class UnregisterService : public BaseService {
        public:
            UnregisterService();
            ~UnregisterService() override = default;

            std::vector<std::string> getSupportedCommands() const override;

            common::Result validateCommand(
                const swan::protocol::ControlCommand& cmd) const override;

        protected:
            common::Result doExecute(
                const swan::protocol::ControlCommand& cmd,
                const std::shared_ptr<command::CommandContext>& context) override;

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