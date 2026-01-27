//
// Created by wave on 2026/1/27.
//
#pragma once

#include <memory>
#include <string>
#include <mutex>
#include "protocol/parser.h"
#include "protocol/dispatch.h"

namespace swan {
    namespace init {

        bool initCommandProcessing(
            bool parser_strict_mode = true,
            size_t dispatcher_max_queue_size = 1000);

        bool shutdownCommandProcessing(std::string& error_msg);

        std::shared_ptr<protocol::ProtocolParser> getProtocolParser();
        std::shared_ptr<protocol::CommandDispatcher> getCommandDispatcher();

        bool isCommandProcessingInitialized();

    } // namespace init
} // namespace swan