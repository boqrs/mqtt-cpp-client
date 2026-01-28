//
// Created by wave on 2026/1/23.
//

#pragma once

#include "base/base_service.h"
#include "protocol.pb.h"

namespace swan {
    namespace services {

        class StreamService : public BaseService {
        public:
            StreamService();
            ~StreamService() override;

            std::vector<std::string> getSupportedCommands() const override;

            common::Result validateCommand(
                const swan::protocol::ControlCommand& cmd) const override;

        protected:
            common::Result doExecute(
                const swan::protocol::ControlCommand& cmd,
                const std::shared_ptr<command::CommandContext>& context) override;

        private:
            // 流会话管理
            struct StreamSession {
                std::string session_id;
                std::string client_ip;
                std::chrono::steady_clock::time_point start_time;
                bool active;  // 改为普通 bool

                // 添加构造函数
                StreamSession() : active(false) {}

                StreamSession(const std::string& id, const std::string& ip,
                            std::chrono::steady_clock::time_point start, bool is_active)
                    : session_id(id), client_ip(ip), start_time(start), active(is_active) {}
            };

            std::unordered_map<std::string, StreamSession> active_sessions_;
            mutable std::mutex sessions_mutex_;
            std::atomic<int> session_counter_{0};

            // 流控制方法
            common::Result startStream(const protocol::StreamControlCmd& stream_cmd,
                                      const std::shared_ptr<command::CommandContext>& context);
            common::Result stopStream(const protocol::StreamControlCmd& stream_cmd,
                                     const std::shared_ptr<command::CommandContext>& context);

            // 生成会话ID
            std::string generateSessionId();

            // 硬件接口
            common::Result startCameraStream(const std::string& session_id);
            common::Result stopCameraStream(const std::string& session_id);
        };

    } // namespace services
} // namespace swan