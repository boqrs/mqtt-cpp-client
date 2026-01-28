//
// Created by wave on 2026/1/27.
//
#pragma once

#include <memory>
#include <string>
#include "protocol/parser.h"
#include "protocol/dispatch.h"

namespace swan {
    namespace init {

        bool initCommandProcessing(
            bool parser_strict_mode = true,
            size_t dispatcher_max_queue_size = 1000);

        bool shutdownCommandProcessing(std::string& error_msg);

        std::shared_ptr<proparser::ProtocolParser> getProtocolParser();
        std::shared_ptr<prodispatcher::CommandDispatcher> getCommandDispatcher();

        bool isCommandProcessingInitialized();
        bool dispatchMqttCommand(const std::string& topic, const std::string& payload);

    } // namespace init
} // namespace swan