//
// Created by wave on 2026/1/21.
//

#pragma once
#include <string>

namespace swan {
    namespace service {

        class LightService {
        public:
            virtual ~LightService() = default;

            virtual bool turnOn(const std::string& deviceId) = 0;
            virtual bool turnOff(const std::string& deviceId) = 0;
            virtual bool isOn(const std::string& deviceId) = 0;
        };

    } // namespace business
} // namespace swan