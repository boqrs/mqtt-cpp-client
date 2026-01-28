//
// Created by wave on 2026/1/23.
//

#pragma once

#include "service/base_service.h"
#include "protocol.pb.h"

namespace swan {
    namespace services {

        class TemperatureService : public BaseService {
        public:
            TemperatureService();
            ~TemperatureService() override = default;

            std::vector<std::string> getSupportedCommands() const override;

            common::Result validateCommand(
                const swan::protocol::ControlCommand& cmd) const override;

        protected:
            common::Result doExecute(
                const swan::protocol::ControlCommand& cmd,
                const std::shared_ptr<command::CommandContext>& context) override;

        private:
            // 温度状态
            struct TemperatureState {
                int32_t platform_temp = 0;
                int32_t right_nozzle_temp = 0;
                int32_t left_nozzle_temp = 0;
                int32_t chamber_temp = 0;

                int32_t platform_target = 0;
                int32_t right_nozzle_target = 0;
                int32_t left_nozzle_target = 0;
                int32_t chamber_target = 0;
            };

            TemperatureState current_state_;
            mutable std::mutex state_mutex_;

            // 温度控制方法
            common::Result setTemperatures(const protocol::TemperatureControlCmd& temp_cmd);
            common::Result validateTemperature(int32_t temp, const std::string& name)const;
            common::Result updateTemperature(int32_t& current, int32_t target, const std::string& name);

            // 硬件接口
            common::Result setHeaterTemperature(const std::string& heater, int32_t target);
            common::Result getTemperature(const std::string& sensor);
        };

    } // namespace services
} // namespace swan