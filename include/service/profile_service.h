//
// Created by wave on 2026/1/23.
//

#pragma once

#include "service/base_service.h"
#include "protocol.pb.h"

namespace swan {
    namespace services {

        class UserService : public BaseService {
        public:
            UserService();
            ~UserService() override = default;

            std::vector<std::string> getSupportedCommands() const override;
            common::Result validateCommand(const ControlCommand& cmd) const override;

        protected:
            common::Result doExecute(
                const ControlCommand& cmd,
                const std::shared_ptr<protocol::command::CommandContext>& context) override;

        private:
            // 用户信息
            struct UserProfile {
                std::string avatar_url;
                std::string user_name;
                std::chrono::system_clock::time_point last_update;
            };

            UserProfile current_profile_;
            mutable std::mutex profile_mutex_;

            // 验证用户数据
            common::Result validateUserProfile(const device::UserProfileCmd& profile_cmd) const;  // 添加 const

            // 更新显示
            common::Result updateUserDisplay();
        };

    } // namespace services
} // namespace swan