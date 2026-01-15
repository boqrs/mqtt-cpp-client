//
// Created by wave on 2026/1/15.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <unordered_map>
#include <chrono>

#include "device_cmd.pb.h"

namespace swan {
    namespace control {

// 前向声明
        class ControlCommand;

/**
 * 材质信息 (Material Slot)
 */
        struct MaterialInfo {
            int32_t slot = 0;           // 料槽编号
            int32_t t_num = 0;          // 工具头编号
            std::string s_rgb;          // 起始颜色
            std::string t_rgb;          // 目标颜色
            std::string material_type;  // 材质类型

            MaterialInfo() = default;
            MaterialInfo(int32_t slot, int32_t t_num, const std::string& s_rgb,
                         const std::string& t_rgb, const std::string& material_type)
                    : slot(slot), t_num(t_num), s_rgb(s_rgb),
                      t_rgb(t_rgb), material_type(material_type) {}

            // 从Protobuf转换
            static MaterialInfo fromProto(const device::control::MaterialInfo& proto);

            // 转换为Protobuf
            device::control::MaterialInfo toProto() const;

            // 检查是否有效
            bool isValid() const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 远程作业参数
 */
        struct NewJobArgs {
            std::string filename;
            std::string file_md5;
            std::string filepath;
            std::string thumb_path;
            bool print_now = false;
            bool leveling = false;
            bool flow_calibration = false;
            bool use_ms = false;
            int32_t t_count = 0;
            std::vector<MaterialInfo> ms;  // 材质信息列表

            NewJobArgs() = default;

            // 从Protobuf转换
            static NewJobArgs fromProto(const device::control::NewJobArgs& proto);

            // 转换为Protobuf
            device::control::NewJobArgs toProto() const;

            // 验证参数
            bool validate() const;

            // 获取材质信息
            std::optional<MaterialInfo> getMaterialBySlot(int32_t slot) const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 作业信息 (SN + UUID)
 */
        struct JobInfo {
            std::string sn;    // 设备序列号
            std::string uuid;  // 作业唯一标识

            JobInfo() = default;
            JobInfo(const std::string& sn, const std::string& uuid)
                    : sn(sn), uuid(uuid) {}

            // 从Protobuf转换
            static JobInfo fromProto(const device::control::JobInfo& proto);

            // 转换为Protobuf
            device::control::JobInfo toProto() const;

            // 检查是否有效
            bool isValid() const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 新远程作业命令
 */
        struct NewJobCmd {
            std::vector<JobInfo> job_info;  // 作业信息列表
            NewJobArgs args;

            NewJobCmd() = default;

            // 从Protobuf转换
            static NewJobCmd fromProto(const device::control::NewJobCmd& proto);

            // 转换为Protobuf
            device::control::NewJobCmd toProto() const;

            // 验证命令
            bool validate() const;

            // 添加作业信息
            void addJobInfo(const std::string& sn, const std::string& uuid);
            void addJobInfo(const JobInfo& info);

            // 获取作业数量
            size_t jobCount() const { return job_info.size(); }

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 本地作业参数
 */
        struct NewLocalJobArgs {
            std::string job_id;
            std::string file_name;
            bool print_now = false;
            bool leveling = false;
            bool flow_calibration = false;
            bool use_ms = false;
            int32_t t_count = 0;
            std::vector<MaterialInfo> ms;  // 材质信息列表

            NewLocalJobArgs() = default;

            // 从Protobuf转换
            static NewLocalJobArgs fromProto(const device::control::NewLocalJobArgs& proto);

            // 转换为Protobuf
            device::control::NewLocalJobArgs toProto() const;

            // 验证参数
            bool validate() const;

            // 获取材质信息
            std::optional<MaterialInfo> getMaterialBySlot(int32_t slot) const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 新本地作业命令
 */
        struct NewLocalJobCmd {
            NewLocalJobArgs args;

            NewLocalJobCmd() = default;

            // 从Protobuf转换
            static NewLocalJobCmd fromProto(const device::control::NewLocalJobCmd& proto);

            // 转换为Protobuf
            device::control::NewLocalJobCmd toProto() const;

            // 验证命令
            bool validate() const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 灯光状态枚举
 */
        enum class LightStatus {
            UNKNOWN = 0,
            OPEN = 1,   // 开灯
            CLOSE = 2   // 关灯
        };

// 枚举转换函数声明
        device::control::LightControlCmd_LightStatus toProtoLightStatus(LightStatus status);
        LightStatus fromProtoLightStatus(device::control::LightControlCmd_LightStatus status);

/**
 * 灯光控制命令
 */
        struct LightControlCmd {
            LightStatus status = LightStatus::UNKNOWN;

            LightControlCmd() = default;
            LightControlCmd(LightStatus status) : status(status) {}

            // 从字符串创建
            static LightControlCmd fromString(const std::string& status_str);

            // 从Protobuf转换
            static LightControlCmd fromProto(const device::control::LightControlCmd& proto);

            // 转换为Protobuf
            device::control::LightControlCmd toProto() const;

            // 转换为字符串
            std::string toString() const;

            // 检查是否有效
            bool isValid() const;
        };

/**
 * 温度控制命令
 */
        struct TemperatureControlCmd {
            int32_t platform = 0;      // 平台温度
            int32_t right_nozzle = 0;  // 右喷嘴温度
            int32_t left_nozzle = 0;   // 左喷嘴温度
            int32_t chamber = 0;       // 腔体温度

            TemperatureControlCmd() = default;
            TemperatureControlCmd(int32_t platform, int32_t right_nozzle,
                                  int32_t left_nozzle, int32_t chamber)
                    : platform(platform), right_nozzle(right_nozzle),
                      left_nozzle(left_nozzle), chamber(chamber) {}

            // 从Protobuf转换
            static TemperatureControlCmd fromProto(const device::control::TemperatureControlCmd& proto);

            // 转换为Protobuf
            device::control::TemperatureControlCmd toProto() const;

            // 验证温度范围
            bool validate() const;

            // 获取最高温度
            int32_t getMaxTemperature() const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 流控制动作枚举
 */
        enum class StreamAction {
            UNKNOWN_ACTION = 0,
            OPEN = 1,   // 开流
            CLOSE = 2   // 关流
        };

// 枚举转换函数声明
        device::control::StreamControlCmd_StreamAction toProtoStreamAction(StreamAction action);
        StreamAction fromProtoStreamAction(device::control::StreamControlCmd_StreamAction action);

/**
 * 流控制命令
 */
        struct StreamControlCmd {
            StreamAction action = StreamAction::UNKNOWN_ACTION;

            StreamControlCmd() = default;
            StreamControlCmd(StreamAction action) : action(action) {}

            // 从字符串创建
            static StreamControlCmd fromString(const std::string& action_str);

            // 从Protobuf转换
            static StreamControlCmd fromProto(const device::control::StreamControlCmd& proto);

            // 转换为Protobuf
            device::control::StreamControlCmd toProto() const;

            // 转换为字符串
            std::string toString() const;

            // 检查是否有效
            bool isValid() const;
        };

/**
 * 用户资料命令
 */
        struct UserProfileCmd {
            std::string avatar;  // 头像URL
            std::string name;    // 用户名

            UserProfileCmd() = default;
            UserProfileCmd(const std::string& avatar, const std::string& name)
                    : avatar(avatar), name(name) {}

            // 从Protobuf转换
            static UserProfileCmd fromProto(const device::control::UserProfileCmd& proto);

            // 转换为Protobuf
            device::control::UserProfileCmd toProto() const;

            // 验证数据
            bool validate() const;

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 设备注销命令
 */
        struct DeviceUnregisterCmd {
            // 空结构体，表示没有参数

            DeviceUnregisterCmd() = default;

            // 从Protobuf转换
            static DeviceUnregisterCmd fromProto(const device::control::DeviceUnregisterCmd& proto);

            // 转换为Protobuf
            device::control::DeviceUnregisterCmd toProto() const;

            // 总是有效
            bool validate() const { return true; }

            // 转换为字符串
            std::string toString() const;
        };

/**
 * 命令参数变体类型
 * 使用 std::variant 存储不同类型的命令参数
 */
        using CommandVariant = std::variant<
                NewJobCmd,
                NewLocalJobCmd,
                LightControlCmd,
                TemperatureControlCmd,
                StreamControlCmd,
                UserProfileCmd,
                DeviceUnregisterCmd
        >;

/**
 * 控制指令类
 * 管理所有类型的控制命令
 */
        class ControlCommand {
        public:
            ControlCommand();
            ~ControlCommand();

            // 禁止拷贝和赋值
            ControlCommand(const ControlCommand&) = delete;
            ControlCommand& operator=(const ControlCommand&) = delete;

            // 移动构造和赋值
            ControlCommand(ControlCommand&& other) noexcept;
            ControlCommand& operator=(ControlCommand&& other) noexcept;

            // 工厂方法：从字符串创建命令
            static std::unique_ptr<ControlCommand> createFromString(
                    const std::string& cmd_type,
                    const std::string& json_args = "");

            // 工厂方法：从Protobuf创建命令
            static std::unique_ptr<ControlCommand> fromProto(const device::control::ControlCommand& proto);

            // 设置命令类型和参数
            void setNewJobCmd(NewJobCmd&& cmd);
            void setNewJobCmd(const NewJobCmd& cmd);

            void setNewLocalJobCmd(NewLocalJobCmd&& cmd);
            void setNewLocalJobCmd(const NewLocalJobCmd& cmd);

            void setLightControlCmd(LightControlCmd&& cmd);
            void setLightControlCmd(const LightControlCmd& cmd);

            void setTemperatureControlCmd(TemperatureControlCmd&& cmd);
            void setTemperatureControlCmd(const TemperatureControlCmd& cmd);

            void setStreamControlCmd(StreamControlCmd&& cmd);
            void setStreamControlCmd(const StreamControlCmd& cmd);

            void setUserProfileCmd(UserProfileCmd&& cmd);
            void setUserProfileCmd(const UserProfileCmd& cmd);

            void setDeviceUnregisterCmd(DeviceUnregisterCmd&& cmd);
            void setDeviceUnregisterCmd(const DeviceUnregisterCmd& cmd);

            // 获取命令类型
            std::string getCommandType() const;

            // 检查命令类型
            bool isNewJobCmd() const;
            bool isNewLocalJobCmd() const;
            bool isLightControlCmd() const;
            bool isTemperatureControlCmd() const;
            bool isStreamControlCmd() const;
            bool isUserProfileCmd() const;
            bool isDeviceUnregisterCmd() const;

            // 获取命令参数（带类型检查）
            template<typename T>
            std::optional<T> getCommandAs() const {
                try {
                    return std::get<T>(command_variant_);
                } catch (const std::bad_variant_access&) {
                    return std::nullopt;
                }
            }

            // 访问命令参数（无类型检查）
            const CommandVariant& getCommandVariant() const { return command_variant_; }

            // 转换为Protobuf
            device::control::ControlCommand toProto() const;

            // 序列化为二进制数据
            std::vector<uint8_t> serializeToBytes() const;

            // 从二进制数据反序列化
            bool parseFromBytes(const std::vector<uint8_t>& data);

            // 序列化为JSON字符串
            std::string serializeToJson() const;

            // 从JSON字符串解析
            bool parseFromJson(const std::string& json_str);

            // 验证命令
            bool validate() const;

            // 转换为可读字符串
            std::string toString() const;

            // 获取命令描述
            std::string getDescription() const;

            // 时间戳
            void setTimestamp(uint64_t timestamp) { timestamp_ = timestamp; }
            uint64_t getTimestamp() const { return timestamp_; }

            // 命令ID（用于追踪）
            void setCommandId(const std::string& id) { command_id_ = id; }
            const std::string& getCommandId() const { return command_id_; }

            // 设备ID
            void setDeviceId(const std::string& id) { device_id_ = id; }
            const std::string& getDeviceId() const { return device_id_; }

        private:
            CommandVariant command_variant_;
            uint64_t timestamp_ = 0;
            std::string command_id_;
            std::string device_id_;
        };

/**
 * 控制命令工厂
 * 用于创建和解析控制命令
 */
        class ControlCommandFactory {
        public:
            ControlCommandFactory() = default;

            // 注册命令处理器
            using CommandParser = std::function<std::unique_ptr<ControlCommand>(const std::string&)>;
            using CommandBuilder = std::function<device::control::ControlCommand(const ControlCommand&)>;

            void registerCommandHandler(const std::string& cmd_type,
                                        CommandParser parser,
                                        CommandBuilder builder);

            // 创建命令
            std::unique_ptr<ControlCommand> createCommand(const std::string& cmd_type,
                                                          const std::string& args_json = "");

            // 序列化命令
            std::vector<uint8_t> serializeCommand(const ControlCommand& command);

            // 反序列化命令
            std::unique_ptr<ControlCommand> deserializeCommand(const std::vector<uint8_t>& data);

            // 获取所有支持的命令类型
            std::vector<std::string> getSupportedCommands() const;

        private:
            std::unordered_map<std::string, std::pair<CommandParser, CommandBuilder>> handlers_;
        };

    } // namespace control
} // namespace swan