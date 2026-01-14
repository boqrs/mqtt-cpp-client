cmake_minimum_required(VERSION 3.14)
project(swan VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 1. 基础配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
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
        -Wno-error
        -Wno-macro-redefined
)

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

# 查找 Paho MQTT C（手动设置路径）
set(PAHO_MQTT_C_INCLUDE_DIRS /usr/local/include/paho-mqtt)
set(PAHO_MQTT_C_LIB /usr/local/lib/libmqttpacket.a)

if(EXISTS ${PAHO_MQTT_C_INCLUDE_DIRS}/MQTTClient.h AND EXISTS ${PAHO_MQTT_C_LIB})
    set(PAHO_MQTT_C_FOUND TRUE)
    message(STATUS "找到 Paho MQTT C: ${PAHO_MQTT_C_INCLUDE_DIRS}")
else()
    set(PAHO_MQTT_C_FOUND FALSE)
    message(WARNING "未找到 Paho MQTT C 库")
endif()

# 查找线程库
find_package(Threads REQUIRED)

# 查找 fmt 库（spdlog 的依赖，必须安装 libfmt-dev）
find_package(fmt REQUIRED)
message(STATUS "找到 fmt 库")

# ------------------------------------------------------------------------------
# 4. 集成 spdlog 选项
# ------------------------------------------------------------------------------
option(ENABLE_SPDLOG "启用 spdlog 日志库" ON)
option(SPDLOG_ASYNC_LOGGING "启用异步日志" OFF)

# spdlog 全局编译定义（应用于所有目标）
if(ENABLE_SPDLOG)
    add_compile_definitions(
            SPDLOG_FMT_EXTERNAL=1      # 使用外部 fmt 库
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
# 5. Protobuf 消息生成
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
# 6. 日志系统库 (embedded_logger)
# ------------------------------------------------------------------------------
file(GLOB LOGGER_SRCS ${SOURCE_ROOT}/logger/*.cpp)
file(GLOB LOGGER_HDRS ${INCLUDE_DIR}/logger/*.h ${INCLUDE_DIR}/logger/**/*.h)

if(LOGGER_SRCS)
    message(STATUS "找到日志源文件，创建日志库...")

    # 第一步：必须先创建目标
    add_library(embedded_logger STATIC ${LOGGER_SRCS})

    # 第二步：配置基本属性
    target_include_directories(embedded_logger PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )


    target_compile_definitions(embedded_logger PRIVATE
            PROJECT_NAME="${PROJECT_NAME}"
            PROJECT_VERSION="${PROJECT_VERSION}"
    )

    # 第三步：配置 spdlog（仅在创建目标后）
    if(ENABLE_SPDLOG)
        message(STATUS "正在为 embedded_logger 配置 spdlog...")

        # 手动指定 spdlog 头文件路径
        target_include_directories(embedded_logger PRIVATE /usr/local/include)

        # 手动链接 spdlog 静态库
        target_link_libraries(embedded_logger PRIVATE /usr/local/lib/libspdlog.a)

        # 链接 fmt 库（解决 spdlog 内部依赖）
        target_link_libraries(embedded_logger PRIVATE fmt::fmt)

        # 启用 spdlog 的编译定义
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
# 7. 通用工具库 (comm)
# ------------------------------------------------------------------------------
file(GLOB_RECURSE UTILS_SRCS
        ${SOURCE_ROOT}/utils/*.cpp
        ${SOURCE_ROOT}/utils/**/*.cpp
        ${SOURCE_ROOT}/utils/*.c
        ${SOURCE_ROOT}/utils/**/*.c
)

if(UTILS_SRCS)
    message(STATUS "找到通用工具源文件，创建通用工具库...")

    add_library(comm STATIC ${UTILS_SRCS})

    target_include_directories(comm PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${Protobuf_INCLUDE_DIRS}
            ${CMAKE_BINARY_DIR}/generated
    )

    target_compile_definitions(comm PUBLIC
            PROJECT_NAME="${PROJECT_NAME}"
            PROJECT_VERSION="${PROJECT_VERSION}"
    )

    target_link_libraries(comm PUBLIC
            proto_messages
            embedded_logger  # 链接日志库
            ${Protobuf_LIBRARIES}
            ${ZLIB_LIBRARIES}
    )

    message(STATUS "通用工具库创建完成: comm")
else()
    message(WARNING "未找到通用工具源文件")
endif()

# ------------------------------------------------------------------------------
# 8. 数据处理器模块 (data_processor)
# ------------------------------------------------------------------------------
file(GLOB DATA_PROCESSOR_SRCS
        ${SOURCE_ROOT}/processor/*.cpp
        ${SOURCE_ROOT}/processor/**/*.cpp
)

if(DATA_PROCESSOR_SRCS)
    message(STATUS "找到数据处理器源文件，创建数据处理器库...")

    add_library(data_processor STATIC ${DATA_PROCESSOR_SRCS})

    target_include_directories(data_processor PUBLIC
            ${INCLUDE_DIR}
            ${PROTO_GEN_DIR}
            ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(data_processor PUBLIC
            comm
            proto_messages
            embedded_logger  # 链接日志库
    )

    message(STATUS "数据处理器库创建完成: data_processor")
else()
    message(WARNING "未找到数据处理器源文件")
endif()

# ------------------------------------------------------------------------------
# 9. MQTT发布器模块 (mqtt_publisher)
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
            comm
            proto_messages
            embedded_logger  # 链接日志库
            ${ZLIB_LIBRARIES}
    )

    if(PAHO_MQTT_C_FOUND)
        target_link_libraries(mqtt_publisher PUBLIC ${PAHO_MQTT_C_LIB})
    endif()

    message(STATUS "MQTT发布器库创建完成: mqtt_publisher")
else()
    message(WARNING "未找到MQTT发布器源文件")
endif()

# ------------------------------------------------------------------------------
# 10. 生成配置头文件
# ------------------------------------------------------------------------------
# 生成项目配置头文件
configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/config/config.h.in
        ${CMAKE_BINARY_DIR}/generated/config.h
        @ONLY
)

# 生成编译配置头文件
file(WRITE ${CMAKE_BINARY_DIR}/generated/build_config.h
        "// 自动生成的构建配置
#pragma once

// 项目信息
#define PROJECT_NAME \"${PROJECT_NAME}\"
#define PROJECT_VERSION \"${PROJECT_VERSION}\"
#define PROJECT_BUILD_TYPE \"${CMAKE_BUILD_TYPE}\"
#define PROJECT_COMPILER \"${CMAKE_CXX_COMPILER_ID}\"

// 构建时间
#define BUILD_TIMESTAMP __TIMESTAMP__

// 功能特性
#ifdef DEBUG_BUILD
#define ENABLE_DEBUG 1
#define LOG_LEVEL_DEBUG 1
#else
#define ENABLE_DEBUG 0
#define LOG_LEVEL_INFO 1
#endif

#if ${ENABLE_SPDLOG}
#define USE_SPDLOG 1
#else
#define USE_SPDLOG 0
#endif

// 辅助宏
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
")

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
)

# 添加 spdlog 头文件路径（确保 CLion 能索引到）
if(ENABLE_SPDLOG)
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/local/include)
endif()

if(PAHO_MQTT_C_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PAHO_MQTT_C_INCLUDE_DIRS})
endif()

# 链接所有库
target_link_libraries(${PROJECT_NAME} PRIVATE
        mqtt_publisher
        data_processor
        comm
        proto_messages
        embedded_logger  # 链接日志库
        Threads::Threads
        ${ZLIB_LIBRARIES}
        ${Protobuf_LIBRARIES}
)

if(PAHO_MQTT_C_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PAHO_MQTT_C_LIB})
endif()

# 链接 fmt 库（通过 embedded_logger 间接链接，这里显式链接确保解析）
if(ENABLE_SPDLOG)
    target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
endif()

# 设置目标属性
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
# 生成编译命令数据库（用于IDE）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加一个目标用于清理生成的文件
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
    message(STATUS "    - Paho MQTT C: 已找到")
else()
    message(STATUS "    - Paho MQTT C: 未找到")
endif()
if(ENABLE_SPDLOG)
    message(STATUS "    - spdlog: 已启用 (手动配置)")
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
if(TARGET comm)
    message(STATUS "    - comm: 通用工具库")
endif()
if(TARGET data_processor)
    message(STATUS "    - data_processor: 数据处理器")
endif()
if(TARGET mqtt_publisher)
    message(STATUS "    - mqtt_publisher: MQTT发布器")
endif()
message(STATUS "")
message(STATUS "  构建命令:")
message(STATUS "    rm -rf build && mkdir build && cd build")
message(STATUS "    cmake .. -DCMAKE_BUILD_TYPE=Debug")
message(STATUS "    make -j\$(nproc)")
message(STATUS "")
message(STATUS "  清理命令:")
message(STATUS "    make clean_all")
message(STATUS "")
message(STATUS "==================================================")