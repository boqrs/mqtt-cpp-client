cmake_minimum_required(VERSION 3.14)
project(swan VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 1. 基础配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 构建类型设置
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "构建类型" FORCE)
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0 -g)
    add_compile_definitions(DEBUG_BUILD=1)
    set(LOG_LEVEL_DEFAULT "DEBUG")
    message(STATUS "构建类型: Debug")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -DNDEBUG)
    set(LOG_LEVEL_DEFAULT "INFO")
    message(STATUS "构建类型: Release")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_compile_options(-O2 -g -DNDEBUG)
    set(LOG_LEVEL_DEFAULT "INFO")
    message(STATUS "构建类型: RelWithDebInfo")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_compile_options(-Os -DNDEBUG)
    set(LOG_LEVEL_DEFAULT "INFO")
    message(STATUS "构建类型: MinSizeRel")
endif()

# 警告选项
add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter  # 减少protobuf警告
        -Wno-error
)

# MQTT 5.0 支持选项
option(ENABLE_MQTT_5 "启用 MQTT 5.0 协议支持" ON)

# ------------------------------------------------------------------------------
# 2. 项目目录结构
# ------------------------------------------------------------------------------
set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/proto_gen)
set(CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

# 创建生成目录
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

# ------------------------------------------------------------------------------
# 3. 依赖查找
# ------------------------------------------------------------------------------
# 查找 Protobuf
find_package(Protobuf REQUIRED)
message(STATUS "Protobuf 版本: ${Protobuf_VERSION}")

# 查找 Zlib
find_package(ZLIB REQUIRED)
message(STATUS "Zlib 版本: ${ZLIB_VERSION_STRING}")

# ====== Paho MQTT C 库配置 ======
# 直接使用已知路径
set(PAHO_MQTT_C_INCLUDE_DIRS "/usr/local/include")
set(PAHO_MQTT_C_LIB_NAMES paho-mqtt3c paho-mqtt3cs paho-mqtt3a paho-mqtt3as)
set(PAHO_MQTT_C_FOUND FALSE)

# 验证头文件存在
if(EXISTS "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTClient.h")
    message(STATUS "找到 MQTTClient.h: ${PAHO_MQTT_C_INCLUDE_DIRS}")

    # 检查 MQTT 5.0 头文件
    if(EXISTS "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTProperties.h")
        message(STATUS "MQTTProperties.h 可用 - 支持 MQTT 5.0")
    else()
        message(WARNING "MQTTProperties.h 未找到，MQTT 5.0 支持可能受限")
    endif()

    # 检查 MQTTSubscribeOpts.h
    if(EXISTS "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTSubscribeOpts.h")
        message(STATUS "MQTTSubscribeOpts.h 可用")
    else()
        message(WARNING "MQTTSubscribeOpts.h 未找到")
    endif()
else()
    message(FATAL_ERROR "未找到 MQTTClient.h，请安装 Paho MQTT C 库")
endif()

# 查找库文件
foreach(lib_name ${PAHO_MQTT_C_LIB_NAMES})
    find_library(PAHO_MQTT_C_LIB_${lib_name}
            NAMES ${lib_name}
            PATHS /usr/local/lib /usr/lib
            NO_DEFAULT_PATH
    )
    if(PAHO_MQTT_C_LIB_${lib_name})
        set(PAHO_MQTT_C_LIB ${PAHO_MQTT_C_LIB_${lib_name}})
        set(PAHO_MQTT_C_FOUND TRUE)
        message(STATUS "找到 Paho MQTT C 库: ${PAHO_MQTT_C_LIB}")
        break()
    endif()
endforeach()

if(NOT PAHO_MQTT_C_FOUND)
    message(FATAL_ERROR "未找到 Paho MQTT C 库文件")
endif()

# 检查版本信息
if(EXISTS "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTClient.h")
    file(READ "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTClient.h" MQTT_H_CONTENT LIMIT 5000)
    if(MQTT_H_CONTENT MATCHES "define PAHO_C_VERSION[[:space:]]+([0-9]+)")
        set(PAHO_VERSION ${CMAKE_MATCH_1})
        message(STATUS "Paho MQTT C 版本: ${PAHO_VERSION}")
        if(${PAHO_VERSION} GREATER_EQUAL 130)  # 1.3.0
            message(STATUS "支持 MQTT 5.0 协议")
            add_compile_definitions(PAHO_MQTT_C_VERSION_1_3=1)
        endif()
    endif()
endif()

if(ENABLE_MQTT_5 AND PAHO_MQTT_C_FOUND)
    if(EXISTS "${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTProperties.h")
        add_compile_definitions(MQTT5=1)
        message(STATUS "启用 MQTT 5.0 协议支持")
    else()
        message(WARNING "MQTT 5.0 已启用，但缺少 MQTTProperties.h")
    endif()
endif()

# 查找线程库
find_package(Threads REQUIRED)

# 查找 fmt 库
find_package(fmt REQUIRED)
message(STATUS "找到 fmt 库")

# ------------------------------------------------------------------------------
# 4. 集成 nlohmann_json 选项
# ------------------------------------------------------------------------------
find_package(nlohmann_json 3.2.0 REQUIRED)

if(NOT nlohmann_json_FOUND)
    add_library(nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "/usr/local/include"
    )
endif()

# ------------------------------------------------------------------------------
# 5. 集成 spdlog 选项
# ------------------------------------------------------------------------------
option(ENABLE_SPDLOG "启用 spdlog 日志库" ON)
option(SPDLOG_ASYNC_LOGGING "启用异步日志" OFF)

if(ENABLE_SPDLOG)
    add_compile_definitions(
            SPDLOG_FMT_EXTERNAL=1
            SPDLOG_NO_THREAD_ID=1
            SPDLOG_NO_ATOMIC_LEVELS=1
            SPDLOG_NO_TLS=1
            SPDLOG_NO_EXCEPTIONS=1
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
    else()
        add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
    endif()

    if(SPDLOG_ASYNC_LOGGING)
        add_compile_definitions(SPDLOG_ASYNC=1)
    endif()
endif()

# ------------------------------------------------------------------------------
# 6. Protobuf 消息生成
# ------------------------------------------------------------------------------
file(GLOB PROTO_FILES ${PROTO_DIR}/*.proto)

if(NOT PROTO_FILES)
    message(FATAL_ERROR "未找到.proto文件，请检查目录: ${PROTO_DIR}")
endif()

message(STATUS "找到proto文件:")
foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME)
    message(STATUS "  - ${proto_name}")
endforeach()

set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name_we ${proto_file} NAME_WE)
    set(proto_src ${PROTO_GEN_DIR}/${proto_name_we}.pb.cc)
    set(proto_hdr ${PROTO_GEN_DIR}/${proto_name_we}.pb.h)

    add_custom_command(
            OUTPUT ${proto_src} ${proto_hdr}
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            ARGS --cpp_out=${PROTO_GEN_DIR} -I${PROTO_DIR} ${proto_file}
            DEPENDS ${proto_file}
            COMMENT "生成 ${proto_name_we}.pb.cc 和 ${proto_name_we}.pb.h"
            VERBATIM
    )

    list(APPEND PROTO_SRCS ${proto_src})
    list(APPEND PROTO_HDRS ${proto_hdr})
endforeach()

add_custom_target(generate_protos ALL
        DEPENDS ${PROTO_SRCS} ${PROTO_HDRS}
        COMMENT "生成所有Protobuf文件"
)

# 创建 Protobuf 库
add_library(proto_messages STATIC ${PROTO_SRCS} ${PROTO_HDRS})
add_dependencies(proto_messages generate_protos)

target_include_directories(proto_messages PUBLIC
        ${PROTO_GEN_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(proto_messages PUBLIC ${Protobuf_LIBRARIES})
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

# ------------------------------------------------------------------------------
# 7. 日志系统库 (embedded_logger)
# ------------------------------------------------------------------------------
file(GLOB LOGGER_SRCS ${SOURCE_ROOT}/logger/*.cpp)
file(GLOB LOGGER_HDRS ${INCLUDE_DIR}/logger/*.h ${INCLUDE_DIR}/logger/**/*.h)

if(LOGGER_SRCS)
    message(STATUS "找到日志源文件，创建日志库...")

    add_library(embedded_logger STATIC ${LOGGER_SRCS})

    target_include_directories(embedded_logger PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )

    target_compile_definitions(embedded_logger PRIVATE
            PROJECT_NAME="${PROJECT_NAME}"
            PROJECT_VERSION="${PROJECT_VERSION}"
    )

    if(ENABLE_SPDLOG)
        message(STATUS "正在为 embedded_logger 配置 spdlog...")
        target_include_directories(embedded_logger PRIVATE /usr/local/include)
        target_link_libraries(embedded_logger PRIVATE /usr/local/lib/libspdlog.a)
        target_link_libraries(embedded_logger PRIVATE fmt::fmt)
        target_compile_definitions(embedded_logger PRIVATE ENABLE_SPDLOG=1)
        message(STATUS "spdlog 配置完成")
    else()
        target_compile_definitions(embedded_logger PRIVATE ENABLE_SPDLOG=0)
    endif()

    target_link_libraries(embedded_logger PUBLIC Threads::Threads)
    message(STATUS "日志库创建完成: embedded_logger")
else()
    message(WARNING "未找到日志源文件，日志功能将不可用")
endif()

# ------------------------------------------------------------------------------
# 8. 协议模块 (protocol)
# ------------------------------------------------------------------------------
file(GLOB PROTOCOL_SRCS
        ${SOURCE_ROOT}/protocol/*.cpp
        ${SOURCE_ROOT}/protocol/**/*.cpp
)

if(PROTOCOL_SRCS)
    message(STATUS "找到协议处理模块源文件，创建协议处理库...")

    add_library(device_protocol STATIC ${PROTOCOL_SRCS})

    target_include_directories(device_protocol PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(device_protocol PUBLIC
            proto_messages
            embedded_logger
    )

    message(STATUS "设备协议管理创建完成: device_protocol")
else()
    message(WARNING "未找到设备协议文件")
endif()

# ------------------------------------------------------------------------------
# 9. 业务模块 (service)
# ------------------------------------------------------------------------------
file(GLOB DEVICE_SERVICE_SRCS
        ${SOURCE_ROOT}/service/*.cpp
        ${SOURCE_ROOT}/service/**/*.cpp
)

if(DEVICE_SERVICE_SRCS)
    message(STATUS "找到设备业务源文件，创建设备业务库...")

    add_library(device_service STATIC ${DEVICE_SERVICE_SRCS})

    target_include_directories(device_service PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(device_service PUBLIC
            proto_messages
            embedded_logger
    )

    message(STATUS "设备服务库创建完成: device_service")
else()
    message(WARNING "未找到设备服务源文件")
endif()

# ------------------------------------------------------------------------------
# 10. MQTT发布器模块 (mqtt_publisher)
# ------------------------------------------------------------------------------
file(GLOB MQTT_PUBLISHER_SRCS
        ${SOURCE_ROOT}/mqtt/*.cpp
        ${SOURCE_ROOT}/mqtt/**/*.cpp
)

if(MQTT_PUBLISHER_SRCS)
    message(STATUS "找到MQTT发布器源文件，创建MQTT发布器库...")

    add_library(mqtt_publisher STATIC ${MQTT_PUBLISHER_SRCS})

    target_include_directories(mqtt_publisher PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )

    if(PAHO_MQTT_C_FOUND)
        target_include_directories(mqtt_publisher PUBLIC ${PAHO_MQTT_C_INCLUDE_DIRS})
    endif()

    target_link_libraries(mqtt_publisher PUBLIC
            device_protocol
            device_service
            proto_messages
            embedded_logger
            ${ZLIB_LIBRARIES}
    )

    if(PAHO_MQTT_C_FOUND)
        target_link_libraries(mqtt_publisher PUBLIC ${PAHO_MQTT_C_LIB})
        # 可能需要链接的其他库
        find_library(DL_LIB dl)
        if(DL_LIB)
            target_link_libraries(mqtt_publisher PUBLIC ${DL_LIB})
        endif()
    endif()

    message(STATUS "MQTT发布器库创建完成: mqtt_publisher")
else()
    message(WARNING "未找到MQTT发布器源文件")
endif()

# ------------------------------------------------------------------------------
# 10. 生成配置头文件
# ------------------------------------------------------------------------------
# 定义生成目录变量，供所有目标使用
set(GENERATED_CONFIG_DIR ${CMAKE_BINARY_DIR}/generated)

set(DEVICE_MODE "ad5x")          # 固定值
set(DEVICE_SN "SNWMOD12332211222")      # 固定序列号

# 确保目录存在
file(MAKE_DIRECTORY ${GENERATED_CONFIG_DIR})

# 核心：生成 config.h (从 .in 模板)
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in
        ${GENERATED_CONFIG_DIR}/config.h
        @ONLY
)
message(STATUS "配置头文件将生成于: ${GENERATED_CONFIG_DIR}/config.h")

# ------------------------------------------------------------------------------
# 11. 主程序
# ------------------------------------------------------------------------------
file(GLOB MAIN_SRCS ${SOURCE_ROOT}/main.cpp)

if(NOT MAIN_SRCS)
    message(FATAL_ERROR "未找到主程序文件: ${SOURCE_ROOT}/main.cpp")
endif()

add_executable(${PROJECT_NAME} ${MAIN_SRCS})
add_dependencies(${PROJECT_NAME} generate_protos)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${INCLUDE_DIR}
        ${PROTO_GEN_DIR}
        ${CMAKE_BINARY_DIR}/generated
        ${GENERATED_CONFIG_DIR}
)

if(ENABLE_SPDLOG)
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/local/include)
endif()

if(PAHO_MQTT_C_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PAHO_MQTT_C_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PAHO_MQTT_C_LIB})
    find_library(DL_LIB dl)
    if(DL_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${DL_LIB})
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
        atomic
        mqtt_publisher
        proto_messages
        embedded_logger
        Threads::Threads
        ${ZLIB_LIBRARIES}
        ${Protobuf_LIBRARIES}
        nlohmann_json::nlohmann_json
)

if(ENABLE_SPDLOG)
    target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        OUTPUT_NAME "swan"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# ------------------------------------------------------------------------------
# 12. 辅助功能
# ------------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(clean_all
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROTO_GEN_DIR}
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/generated
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/compile_commands.json
        COMMENT "清理所有生成的文件"
)

# ------------------------------------------------------------------------------
# 13. 项目信息汇总
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "==================================================")
message(STATUS "  项目配置完成")
message(STATUS "  项目: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  输出目录: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "  Protobuf生成目录: ${PROTO_GEN_DIR}")
message(STATUS "")
message(STATUS "  依赖库:")
message(STATUS "    - Protobuf: ${Protobuf_VERSION}")
message(STATUS "    - Zlib: ${ZLIB_VERSION_STRING}")
if(PAHO_MQTT_C_FOUND)
    message(STATUS "    - Paho MQTT C: 已找到 (v1.3.12)")
    if(ENABLE_MQTT_5)
        message(STATUS "      MQTT 5.0: 已启用")
    else()
        message(STATUS "      MQTT 5.0: 未启用")
    endif()
else()
    message(STATUS "    - Paho MQTT C: 未找到")
endif()
if(ENABLE_SPDLOG)
    message(STATUS "    - spdlog: 已启用")
    message(STATUS "    - fmt: 已找到")
else()
    message(STATUS "    - spdlog: 未启用")
endif()
message(STATUS "")
message(STATUS "  项目模块:")
message(STATUS "    - proto_messages: Protobuf消息库")
if(TARGET embedded_logger)
    message(STATUS "    - embedded_logger: 日志系统")
endif()
if(TARGET device_protocol)
    message(STATUS "    - device_protocol: 设备协议")
endif()
if(TARGET device_service)
    message(STATUS "    - device_service: 设备命令业务")
endif()
if(TARGET mqtt_publisher)
    message(STATUS "    - mqtt_publisher: MQTT发布器")
endif()
message(STATUS "")
message(STATUS "  构建命令:")
message(STATUS "    rm -rf build && mkdir build && cd build")
message(STATUS "    cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_MQTT_5=ON")
message(STATUS "    make -j\$(nproc)")
message(STATUS "")
message(STATUS "  清理命令:")
message(STATUS "    make clean_all")
message(STATUS "")
message(STATUS "==================================================")