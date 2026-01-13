cmake_minimum_required(VERSION 3.15)
project(swan VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# 1. 基础配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra)
else()
    add_compile_options(-O2)
endif()

# 如果是Clang，添加libc++支持
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "使用Clang编译器: ${CMAKE_CXX_COMPILER_VERSION}")
endif()

# ------------------------------------------------------------------------------
# 2. 目录结构
# ------------------------------------------------------------------------------
set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDE_DIR ${SOURCE_ROOT}/include)
set(SRC_DIR ${SOURCE_ROOT}/src)
set(PROTO_DIR ${SOURCE_ROOT}/proto)

# Protobuf生成目录
set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/generated/proto)

# ------------------------------------------------------------------------------
# 3. 依赖查找
# ------------------------------------------------------------------------------
# 必需依赖
find_package(Threads REQUIRED)

# Protobuf
find_package(Protobuf REQUIRED)
message(STATUS "Protobuf版本: ${Protobuf_VERSION}")
message(STATUS "Protobuf包含目录: ${Protobuf_INCLUDE_DIRS}")
message(STATUS "Protobuf库: ${Protobuf_LIBRARIES}")

# zlib（修复链接错误）
find_package(ZLIB REQUIRED)
message(STATUS "zlib版本: ${ZLIB_VERSION_STRING}")

# Paho MQTT C库
find_library(PAHO_MQTT_C_LIB NAMES paho-mqtt3cs paho-mqtt3c libmqttpacket.a
        PATHS /usr/local/lib /opt/local/lib)
if(PAHO_MQTT_C_LIB)
    message(STATUS "找到Paho MQTT C库: ${PAHO_MQTT_C_LIB}")
else()
    message(WARNING "Paho MQTT C库未找到，使用绝对路径")
    set(PAHO_MQTT_C_LIB "/usr/local/lib/libmqttpacket.a")
endif()

# ------------------------------------------------------------------------------
# 4. Protobuf生成
# ------------------------------------------------------------------------------
# 确保生成目录存在
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# 查找所有.proto文件
file(GLOB PROTO_FILES ${PROTO_DIR}/*.proto)

if(NOT PROTO_FILES)
    message(FATAL_ERROR "未找到.proto文件")
endif()

message(STATUS "找到proto文件: ${PROTO_FILES}")

# 为每个.proto文件生成C++代码
set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(proto_file ${PROTO_FILES})
    # 获取文件名（不带扩展名）
    get_filename_component(proto_name_we ${proto_file} NAME_WE)

    # 设置生成的源文件和头文件路径
    set(proto_src ${PROTO_GEN_DIR}/${proto_name_we}.pb.cc)
    set(proto_hdr ${PROTO_GEN_DIR}/${proto_name_we}.pb.h)

    message(STATUS "生成protobuf文件: ${proto_name_we}.pb.{h,cc}")

    # 添加自定义命令
    add_custom_command(
            OUTPUT ${proto_src} ${proto_hdr}
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            ARGS --cpp_out=${PROTO_GEN_DIR}
            -I${PROTO_DIR}
            ${proto_file}
            DEPENDS ${proto_file}
            COMMENT "生成 ${proto_name_we}.pb.h 和 ${proto_name_we}.pb.cc"
    )

    # 添加到列表
    list(APPEND PROTO_SRCS ${proto_src})
    list(APPEND PROTO_HDRS ${proto_hdr})
endforeach()

# 创建自定义目标确保proto文件被生成
add_custom_target(generate_protos ALL
        DEPENDS ${PROTO_SRCS} ${PROTO_HDRS}
)

# 创建Protobuf消息库
add_library(proto_messages STATIC ${PROTO_SRCS} ${PROTO_HDRS})

# 添加对自定义目标的依赖
add_dependencies(proto_messages generate_protos)

# 设置包含目录
target_include_directories(proto_messages PUBLIC
        ${PROTO_GEN_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

# 链接Protobuf库
target_link_libraries(proto_messages PUBLIC ${Protobuf_LIBRARIES})

# 标记为生成文件
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS}
        PROPERTIES GENERATED TRUE
)

# ------------------------------------------------------------------------------
# 5. 全局包含路径
# ------------------------------------------------------------------------------
include_directories(
        ${INCLUDE_DIR}
        ${PROTO_GEN_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

# ------------------------------------------------------------------------------
# 6. 通用工具库 (comm) - 包含utils下的所有通用代码
# ------------------------------------------------------------------------------
# 查找utils目录下的所有源文件
file(GLOB_RECURSE UTILS_SRCS
        ${SRC_DIR}/utils/*.cpp
        ${SRC_DIR}/utils/**/*.cpp
)

# 查找utils目录下的所有头文件
file(GLOB_RECURSE UTILS_HDRS
        ${INCLUDE_DIR}/utils/*.h
        ${INCLUDE_DIR}/utils/**/*.h
)

# 检查是否有utils源文件
if(NOT UTILS_SRCS)
    message(WARNING "未找到utils源文件，请检查路径: ${SRC_DIR}/utils/")
else()
    message(STATUS "找到utils源文件: ${UTILS_SRCS}")
endif()

# 创建通用工具库
add_library(comm STATIC ${UTILS_SRCS})

# 设置包含目录
target_include_directories(comm PUBLIC
        ${INCLUDE_DIR}
        ${PROTO_GEN_DIR}
        ${Protobuf_INCLUDE_DIRS}
)

# 链接依赖
target_link_libraries(comm PUBLIC
        proto_messages
        ${Protobuf_LIBRARIES}
)

# ------------------------------------------------------------------------------
# 7. 项目模块
# ------------------------------------------------------------------------------
# 模块1: 数据处理器
add_library(data_processor STATIC
        ${SRC_DIR}/processor/data_validator.cpp
        ${SRC_DIR}/processor/data_transformer.cpp
)

target_include_directories(data_processor PUBLIC
        ${INCLUDE_DIR}
        ${PROTO_GEN_DIR}
)

target_link_libraries(data_processor PUBLIC
        comm            # 依赖通用工具库
        proto_messages
)

# 模块2: MQTT发布器
add_library(mqtt_publisher STATIC
        ${SRC_DIR}/mqtt/impl.cpp
        ${SRC_DIR}/mqtt/client.cpp
        ${SRC_DIR}/mqtt/thread.cpp
        ${SRC_DIR}/mqtt/device_status_publisher.cpp
)

target_include_directories(mqtt_publisher PUBLIC
        ${INCLUDE_DIR}
        ${PROTO_GEN_DIR}
        /usr/local/include/paho-mqtt
)

target_link_libraries(mqtt_publisher PUBLIC
        comm            # 依赖通用工具库
        proto_messages
        ${ZLIB_LIBRARIES}
)

# ------------------------------------------------------------------------------
# 8. 主程序
# ------------------------------------------------------------------------------
add_executable(${PROJECT_NAME} ${SOURCE_ROOT}/main.cpp)

# 确保主程序依赖proto生成
add_dependencies(${PROJECT_NAME} generate_protos)

# 链接依赖 - 注意链接顺序很重要！
target_link_libraries(${PROJECT_NAME} PRIVATE
        mqtt_publisher
        data_processor
        comm            # 链接通用工具库
        proto_messages  # proto_messages 应该在最后，因为它被其他库依赖
        Threads::Threads
        ${PAHO_MQTT_C_LIB}
        ${ZLIB_LIBRARIES}
        ${Protobuf_LIBRARIES}  # 确保链接 protobuf 库
)

# 输出设置
set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        OUTPUT_NAME "swan"
)

# ------------------------------------------------------------------------------
# 9. 安装配置
# ------------------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

install(DIRECTORY ${INCLUDE_DIR}/ DESTINATION include/swan
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ------------------------------------------------------------------------------
# 10. 编译数据库（IDE支持）
# ------------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# 11. 最终信息
# ------------------------------------------------------------------------------
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "项目配置完成")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  输出目录: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "  Proto生成目录: ${PROTO_GEN_DIR}")
if(UTILS_SRCS)
    message(STATUS "  Utils源文件数量: ${UTILS_SRCS}")
    foreach(src ${UTILS_SRCS})
        get_filename_component(src_name ${src} NAME)
        message(STATUS "    - ${src_name}")
    endforeach()
endif()
message(STATUS "  库依赖:")
message(STATUS "    - proto_messages: Protobuf生成的消息库")
message(STATUS "    - comm: 通用工具库 (utils)")
message(STATUS "    - data_processor: 数据处理器")
message(STATUS "    - mqtt_publisher: MQTT发布器")
message(STATUS "  外部依赖: Protobuf, zlib, Paho MQTT C, Threads")
message(STATUS "==========================================")