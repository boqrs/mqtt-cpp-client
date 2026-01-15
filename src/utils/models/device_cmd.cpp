//
// Created by wave on 2026/1/15.
//
#include <google/protobuf/util/json_util.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

#include "utils/models/device_cmd.h"

using namespace std::chrono;

namespace swan {
    namespace control {

// ============================================================================
// 枚举转换函数实现
// ============================================================================

        device::control::LightControlCmd_LightStatus toProtoLightStatus(LightStatus status) {
            switch (status) {
                case LightStatus::OPEN:
                    return device::control::LightControlCmd_LightStatus_OPEN;
                case LightStatus::CLOSE:
                    return device::control::LightControlCmd_LightStatus_CLOSE;
                default:
                    return device::control::LightControlCmd_LightStatus_UNKNOWN;
            }
        }

        LightStatus fromProtoLightStatus(device::control::LightControlCmd_LightStatus status) {
            switch (status) {
                case device::control::LightControlCmd_LightStatus_OPEN:
                    return LightStatus::OPEN;
                case device::control::LightControlCmd_LightStatus_CLOSE:
                    return LightStatus::CLOSE;
                default:
                    return LightStatus::UNKNOWN;
            }
        }

        device::control::StreamControlCmd_StreamAction toProtoStreamAction(StreamAction action) {
            switch (action) {
                case StreamAction::OPEN:
                    return device::control::StreamControlCmd_StreamAction_OPEN;
                case StreamAction::CLOSE:
                    return device::control::StreamControlCmd_StreamAction_CLOSE;
                default:
                    return device::control::StreamControlCmd_StreamAction_UNKNOWN_ACTION;
            }
        }

        StreamAction fromProtoStreamAction(device::control::StreamControlCmd_StreamAction action) {
            switch (action) {
                case device::control::StreamControlCmd_StreamAction_OPEN:
                    return StreamAction::OPEN;
                case device::control::StreamControlCmd_StreamAction_CLOSE:
                    return StreamAction::CLOSE;
                default:
                    return StreamAction::UNKNOWN_ACTION;
            }
        }

// ============================================================================
// MaterialInfo 实现
// ============================================================================

        MaterialInfo MaterialInfo::fromProto(const device::control::MaterialInfo& proto) {
            MaterialInfo info;
            info.slot = proto.slot();
            info.t_num = proto.t_num();
            info.s_rgb = proto.s_rgb();
            info.t_rgb = proto.t_rgb();
            info.material_type = proto.material_type();
            return info;
        }

        device::control::MaterialInfo MaterialInfo::toProto() const {
            device::control::MaterialInfo proto;
            proto.set_slot(slot);
            proto.set_t_num(t_num);
            proto.set_s_rgb(s_rgb);
            proto.set_t_rgb(t_rgb);
            proto.set_material_type(material_type);
            return proto;
        }

        bool MaterialInfo::isValid() const {
            return slot >= 0 && t_num >= 0 && !material_type.empty();
        }

        std::string MaterialInfo::toString() const {
            std::stringstream ss;
            ss << "MaterialInfo{slot=" << slot
               << ", t_num=" << t_num
               << ", s_rgb='" << s_rgb << "'"
               << ", t_rgb='" << t_rgb << "'"
               << ", material_type='" << material_type << "'"
               << "}";
            return ss.str();
        }

// ============================================================================
// NewJobArgs 实现
// ============================================================================

        NewJobArgs NewJobArgs::fromProto(const device::control::NewJobArgs& proto) {
            NewJobArgs args;
            args.filename = proto.filename();
            args.file_md5 = proto.file_md5();
            args.filepath = proto.filepath();
            args.thumb_path = proto.thumb_path();
            args.print_now = proto.print_now();
            args.leveling = proto.leveling();
            args.flow_calibration = proto.flow_calibration();
            args.use_ms = proto.use_ms();
            args.t_count = proto.t_count();

            // 转换材质信息列表
            args.ms.reserve(proto.ms_size());
            for (int i = 0; i < proto.ms_size(); i++) {
                args.ms.push_back(MaterialInfo::fromProto(proto.ms(i)));
            }

            return args;
        }

        device::control::NewJobArgs NewJobArgs::toProto() const {
            device::control::NewJobArgs proto;
            proto.set_filename(filename);
            proto.set_file_md5(file_md5);
            proto.set_filepath(filepath);
            proto.set_thumb_path(thumb_path);
            proto.set_print_now(print_now);
            proto.set_leveling(leveling);
            proto.set_flow_calibration(flow_calibration);
            proto.set_use_ms(use_ms);
            proto.set_t_count(t_count);

            // 添加材质信息
            for (const auto& material : ms) {
                auto* material_proto = proto.add_ms();
                *material_proto = material.toProto();
            }

            return proto;
        }

        bool NewJobArgs::validate() const {
            if (filename.empty() || file_md5.empty() || filepath.empty()) {
                return false;
            }

            // 验证MD5格式
            if (file_md5.length() != 32) {
                return false;
            }

            // 如果使用材质，验证材质信息
            if (use_ms) {
                for (const auto& material : ms) {
                    if (!material.isValid()) {
                        return false;
                    }
                }
            }

            return true;
        }

        std::optional<MaterialInfo> NewJobArgs::getMaterialBySlot(int32_t slot) const {
            for (const auto& material : ms) {
                if (material.slot == slot) {
                    return material;
                }
            }
            return std::nullopt;
        }

        std::string NewJobArgs::toString() const {
            std::stringstream ss;
            ss << "NewJobArgs{filename='" << filename
               << "', file_md5='" << file_md5
               << "', filepath='" << filepath
               << "', thumb_path='" << thumb_path
               << "', print_now=" << (print_now ? "true" : "false")
               << ", leveling=" << (leveling ? "true" : "false")
               << ", flow_calibration=" << (flow_calibration ? "true" : "false")
               << ", use_ms=" << (use_ms ? "true" : "false")
               << ", t_count=" << t_count
               << ", ms_count=" << ms.size()
               << "}";
            return ss.str();
        }

// ============================================================================
// JobInfo 实现
// ============================================================================

        JobInfo JobInfo::fromProto(const device::control::JobInfo& proto) {
            JobInfo info;
            info.sn = proto.sn();
            info.uuid = proto.uuid();
            return info;
        }

        device::control::JobInfo JobInfo::toProto() const {
            device::control::JobInfo proto;
            proto.set_sn(sn);
            proto.set_uuid(uuid);
            return proto;
        }

        bool JobInfo::isValid() const {
            return !sn.empty() && !uuid.empty();
        }

        std::string JobInfo::toString() const {
            std::stringstream ss;
            ss << "JobInfo{sn='" << sn << "', uuid='" << uuid << "'}";
            return ss.str();
        }

// ============================================================================
// NewJobCmd 实现
// ============================================================================

        NewJobCmd NewJobCmd::fromProto(const device::control::NewJobCmd& proto) {
            NewJobCmd cmd;

            // 转换作业信息列表
            cmd.job_info.reserve(proto.job_info_size());
            for (int i = 0; i < proto.job_info_size(); i++) {
                cmd.job_info.push_back(JobInfo::fromProto(proto.job_info(i)));
            }

            // 转换参数
            cmd.args = NewJobArgs::fromProto(proto.args());

            return cmd;
        }

        device::control::NewJobCmd NewJobCmd::toProto() const {
            device::control::NewJobCmd proto;

            // 添加作业信息
            for (const auto& job_info : job_info) {
                auto* job_info_proto = proto.add_job_info();
                *job_info_proto = job_info.toProto();
            }

            // 设置参数
            *proto.mutable_args() = args.toProto();

            return proto;
        }

        bool NewJobCmd::validate() const {
            if (job_info.empty()) {
                return false;
            }

            for (const auto& info : job_info) {
                if (!info.isValid()) {
                    return false;
                }
            }

            return args.validate();
        }

        void NewJobCmd::addJobInfo(const std::string& sn, const std::string& uuid) {
            job_info.emplace_back(sn, uuid);
        }

        void NewJobCmd::addJobInfo(const JobInfo& info) {
            job_info.push_back(info);
        }

        std::string NewJobCmd::toString() const {
            std::stringstream ss;
            ss << "NewJobCmd{job_count=" << job_info.size()
               << ", args=" << args.toString() << "}";
            return ss.str();
        }

// ============================================================================
// NewLocalJobArgs 实现
// ============================================================================

        NewLocalJobArgs NewLocalJobArgs::fromProto(const device::control::NewLocalJobArgs& proto) {
            NewLocalJobArgs args;
            args.job_id = proto.job_id();
            args.file_name = proto.file_name();
            args.print_now = proto.print_now();
            args.leveling = proto.leveling();
            args.flow_calibration = proto.flow_calibration();
            args.use_ms = proto.use_ms();
            args.t_count = proto.t_count();

            // 转换材质信息列表
            args.ms.reserve(proto.ms_size());
            for (int i = 0; i < proto.ms_size(); i++) {
                args.ms.push_back(MaterialInfo::fromProto(proto.ms(i)));
            }

            return args;
        }

        device::control::NewLocalJobArgs NewLocalJobArgs::toProto() const {
            device::control::NewLocalJobArgs proto;
            proto.set_job_id(job_id);
            proto.set_file_name(file_name);
            proto.set_print_now(print_now);
            proto.set_leveling(leveling);
            proto.set_flow_calibration(flow_calibration);
            proto.set_use_ms(use_ms);
            proto.set_t_count(t_count);

            // 添加材质信息
            for (const auto& material : ms) {
                auto* material_proto = proto.add_ms();
                *material_proto = material.toProto();
            }

            return proto;
        }

        bool NewLocalJobArgs::validate() const {
            if (job_id.empty() || file_name.empty()) {
                return false;
            }

            // 如果使用材质，验证材质信息
            if (use_ms) {
                for (const auto& material : ms) {
                    if (!material.isValid()) {
                        return false;
                    }
                }
            }

            return true;
        }

        std::optional<MaterialInfo> NewLocalJobArgs::getMaterialBySlot(int32_t slot) const {
            for (const auto& material : ms) {
                if (material.slot == slot) {
                    return material;
                }
            }
            return std::nullopt;
        }

        std::string NewLocalJobArgs::toString() const {
            std::stringstream ss;
            ss << "NewLocalJobArgs{job_id='" << job_id
               << "', file_name='" << file_name
               << "', print_now=" << (print_now ? "true" : "false")
               << ", leveling=" << (leveling ? "true" : "false")
               << ", flow_calibration=" << (flow_calibration ? "true" : "false")
               << ", use_ms=" << (use_ms ? "true" : "false")
               << ", t_count=" << t_count
               << ", ms_count=" << ms.size()
               << "}";
            return ss.str();
        }

// ============================================================================
// NewLocalJobCmd 实现
// ============================================================================

        NewLocalJobCmd NewLocalJobCmd::fromProto(const device::control::NewLocalJobCmd& proto) {
            NewLocalJobCmd cmd;
            cmd.args = NewLocalJobArgs::fromProto(proto.args());
            return cmd;
        }

        device::control::NewLocalJobCmd NewLocalJobCmd::toProto() const {
            device::control::NewLocalJobCmd proto;
            *proto.mutable_args() = args.toProto();
            return proto;
        }

        bool NewLocalJobCmd::validate() const {
            return args.validate();
        }

        std::string NewLocalJobCmd::toString() const {
            std::stringstream ss;
            ss << "NewLocalJobCmd{" << args.toString() << "}";
            return ss.str();
        }

// ============================================================================
// LightControlCmd 实现
// ============================================================================

        LightControlCmd LightControlCmd::fromString(const std::string& status_str) {
            if (status_str == "open") {
                return LightControlCmd(LightStatus::OPEN);
            } else if (status_str == "close") {
                return LightControlCmd(LightStatus::CLOSE);
            }
            return LightControlCmd(LightStatus::UNKNOWN);
        }

        LightControlCmd LightControlCmd::fromProto(const device::control::LightControlCmd& proto) {
            LightControlCmd cmd;
            cmd.status = fromProtoLightStatus(proto.status());
            return cmd;
        }

        device::control::LightControlCmd LightControlCmd::toProto() const {
            device::control::LightControlCmd proto;
            proto.set_status(toProtoLightStatus(status));
            return proto;
        }

        std::string LightControlCmd::toString() const {
            switch (status) {
                case LightStatus::OPEN: return "LightControlCmd{status=OPEN}";
                case LightStatus::CLOSE: return "LightControlCmd{status=CLOSE}";
                default: return "LightControlCmd{status=UNKNOWN}";
            }
        }

        bool LightControlCmd::isValid() const {
            return status != LightStatus::UNKNOWN;
        }

// ============================================================================
// TemperatureControlCmd 实现
// ============================================================================

        TemperatureControlCmd TemperatureControlCmd::fromProto(
                const device::control::TemperatureControlCmd& proto) {
            TemperatureControlCmd cmd;
            cmd.platform = proto.platform();
            cmd.right_nozzle = proto.right_nozzle();
            cmd.left_nozzle = proto.left_nozzle();
            cmd.chamber = proto.chamber();
            return cmd;
        }

        device::control::TemperatureControlCmd TemperatureControlCmd::toProto() const {
            device::control::TemperatureControlCmd proto;
            proto.set_platform(platform);
            proto.set_right_nozzle(right_nozzle);
            proto.set_left_nozzle(left_nozzle);
            proto.set_chamber(chamber);
            return proto;
        }

        bool TemperatureControlCmd::validate() const {
            // 温度范围检查（示例值，请根据实际情况调整）
            const int32_t MIN_TEMP = 0;
            const int32_t MAX_TEMP = 300;

            return platform >= MIN_TEMP && platform <= MAX_TEMP &&
                   right_nozzle >= MIN_TEMP && right_nozzle <= MAX_TEMP &&
                   left_nozzle >= MIN_TEMP && left_nozzle <= MAX_TEMP &&
                   chamber >= MIN_TEMP && chamber <= MAX_TEMP;
        }

        int32_t TemperatureControlCmd::getMaxTemperature() const {
            return std::max({platform, right_nozzle, left_nozzle, chamber});
        }

        std::string TemperatureControlCmd::toString() const {
            std::stringstream ss;
            ss << "TemperatureControlCmd{"
               << "platform=" << platform
               << ", right_nozzle=" << right_nozzle
               << ", left_nozzle=" << left_nozzle
               << ", chamber=" << chamber
               << "}";
            return ss.str();
        }

// ============================================================================
// StreamControlCmd 实现
// ============================================================================

        StreamControlCmd StreamControlCmd::fromString(const std::string& action_str) {
            if (action_str == "open") {
                return StreamControlCmd(StreamAction::OPEN);
            } else if (action_str == "close") {
                return StreamControlCmd(StreamAction::CLOSE);
            }
            return StreamControlCmd(StreamAction::UNKNOWN_ACTION);
        }

        StreamControlCmd StreamControlCmd::fromProto(const device::control::StreamControlCmd& proto) {
            StreamControlCmd cmd;
            cmd.action = fromProtoStreamAction(proto.action());
            return cmd;
        }

        device::control::StreamControlCmd StreamControlCmd::toProto() const {
            device::control::StreamControlCmd proto;
            proto.set_action(toProtoStreamAction(action));
            return proto;
        }

        std::string StreamControlCmd::toString() const {
            switch (action) {
                case StreamAction::OPEN: return "StreamControlCmd{action=OPEN}";
                case StreamAction::CLOSE: return "StreamControlCmd{action=CLOSE}";
                default: return "StreamControlCmd{action=UNKNOWN}";
            }
        }

        bool StreamControlCmd::isValid() const {
            return action != StreamAction::UNKNOWN_ACTION;
        }

// ============================================================================
// UserProfileCmd 实现
// ============================================================================

        UserProfileCmd UserProfileCmd::fromProto(const device::control::UserProfileCmd& proto) {
            UserProfileCmd cmd;
            cmd.avatar = proto.avatar();
            cmd.name = proto.name();
            return cmd;
        }

        device::control::UserProfileCmd UserProfileCmd::toProto() const {
            device::control::UserProfileCmd proto;
            proto.set_avatar(avatar);
            proto.set_name(name);
            return proto;
        }

        bool UserProfileCmd::validate() const {
            return !name.empty();
        }

        std::string UserProfileCmd::toString() const {
            std::stringstream ss;
            ss << "UserProfileCmd{name='" << name << "', avatar='" << avatar << "'}";
            return ss.str();
        }

// ============================================================================
// DeviceUnregisterCmd 实现
// ============================================================================

        DeviceUnregisterCmd DeviceUnregisterCmd::fromProto(
                const device::control::DeviceUnregisterCmd& proto) {
            (void)proto; // 明确标记参数未使用，避免警告
            return DeviceUnregisterCmd();
        }

        device::control::DeviceUnregisterCmd DeviceUnregisterCmd::toProto() const {
            return device::control::DeviceUnregisterCmd();
        }

        std::string DeviceUnregisterCmd::toString() const {
            return "DeviceUnregisterCmd{}";
        }

// ============================================================================
// ControlCommand 实现
// ============================================================================

        ControlCommand::ControlCommand()
                : timestamp_(duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count()) {
        }

        ControlCommand::~ControlCommand() = default;

        ControlCommand::ControlCommand(ControlCommand&& other) noexcept
                : command_variant_(std::move(other.command_variant_)),
                  timestamp_(other.timestamp_),
                  command_id_(std::move(other.command_id_)),
                  device_id_(std::move(other.device_id_)) {
        }

        ControlCommand& ControlCommand::operator=(ControlCommand&& other) noexcept {
            if (this != &other) {
                command_variant_ = std::move(other.command_variant_);
                timestamp_ = other.timestamp_;
                command_id_ = std::move(other.command_id_);
                device_id_ = std::move(other.device_id_);
            }
            return *this;
        }

// 工厂方法
        std::unique_ptr<ControlCommand> ControlCommand::createFromString(
                const std::string& cmd_type, const std::string& json_args) {

            (void)json_args; // 暂时不使用JSON参数

            auto cmd = std::make_unique<ControlCommand>();

            if (cmd_type == "newJob_cmd") {
                cmd->setNewJobCmd(NewJobCmd());
            } else if (cmd_type == "newLocalJob_cmd") {
                cmd->setNewLocalJobCmd(NewLocalJobCmd());
            } else if (cmd_type == "lightControl_cmd") {
                cmd->setLightControlCmd(LightControlCmd());
            } else if (cmd_type == "temperatureCtl_cmd") {
                cmd->setTemperatureControlCmd(TemperatureControlCmd());
            } else if (cmd_type == "streamCtrl_cmd") {
                cmd->setStreamControlCmd(StreamControlCmd());
            } else if (cmd_type == "userProfile_cmd") {
                cmd->setUserProfileCmd(UserProfileCmd());
            } else if (cmd_type == "deviceUnregister_cmd") {
                cmd->setDeviceUnregisterCmd(DeviceUnregisterCmd());
            }

            return cmd;
        }

        std::unique_ptr<ControlCommand> ControlCommand::fromProto(
                const device::control::ControlCommand& proto) {

            auto cmd = std::make_unique<ControlCommand>();

            // 根据oneof字段设置命令
            switch (proto.command_args_case()) {
                case device::control::ControlCommand::kNewJob:
                    cmd->setNewJobCmd(NewJobCmd::fromProto(proto.new_job()));
                    break;
                case device::control::ControlCommand::kNewLocalJob:
                    cmd->setNewLocalJobCmd(NewLocalJobCmd::fromProto(proto.new_local_job()));
                    break;
                case device::control::ControlCommand::kLightControl:
                    cmd->setLightControlCmd(LightControlCmd::fromProto(proto.light_control()));
                    break;
                case device::control::ControlCommand::kTemperatureControl:
                    cmd->setTemperatureControlCmd(
                            TemperatureControlCmd::fromProto(proto.temperature_control()));
                    break;
                case device::control::ControlCommand::kStreamControl:
                    cmd->setStreamControlCmd(StreamControlCmd::fromProto(proto.stream_control()));
                    break;
                case device::control::ControlCommand::kUserProfile:
                    cmd->setUserProfileCmd(UserProfileCmd::fromProto(proto.user_profile()));
                    break;
                case device::control::ControlCommand::kDeviceUnregister:
                    cmd->setDeviceUnregisterCmd(DeviceUnregisterCmd::fromProto(proto.device_unregister()));
                    break;
                default:
                    // 未知命令
                    break;
            }

            return cmd;
        }

// 设置命令方法
        void ControlCommand::setNewJobCmd(NewJobCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setNewJobCmd(const NewJobCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setNewLocalJobCmd(NewLocalJobCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setNewLocalJobCmd(const NewLocalJobCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setLightControlCmd(LightControlCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setLightControlCmd(const LightControlCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setTemperatureControlCmd(TemperatureControlCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setTemperatureControlCmd(const TemperatureControlCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setStreamControlCmd(StreamControlCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setStreamControlCmd(const StreamControlCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setUserProfileCmd(UserProfileCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setUserProfileCmd(const UserProfileCmd& cmd) {
            command_variant_ = cmd;
        }

        void ControlCommand::setDeviceUnregisterCmd(DeviceUnregisterCmd&& cmd) {
            command_variant_ = std::move(cmd);
        }

        void ControlCommand::setDeviceUnregisterCmd(const DeviceUnregisterCmd& cmd) {
            command_variant_ = cmd;
        }

// 获取命令类型
        std::string ControlCommand::getCommandType() const {
            if (isNewJobCmd()) return "newJob_cmd";
            if (isNewLocalJobCmd()) return "newLocalJob_cmd";
            if (isLightControlCmd()) return "lightControl_cmd";
            if (isTemperatureControlCmd()) return "temperatureCtl_cmd";
            if (isStreamControlCmd()) return "streamCtrl_cmd";
            if (isUserProfileCmd()) return "userProfile_cmd";
            if (isDeviceUnregisterCmd()) return "deviceUnregister_cmd";
            return "unknown";
        }

// 检查命令类型
        bool ControlCommand::isNewJobCmd() const {
            return std::holds_alternative<NewJobCmd>(command_variant_);
        }

        bool ControlCommand::isNewLocalJobCmd() const {
            return std::holds_alternative<NewLocalJobCmd>(command_variant_);
        }

        bool ControlCommand::isLightControlCmd() const {
            return std::holds_alternative<LightControlCmd>(command_variant_);
        }

        bool ControlCommand::isTemperatureControlCmd() const {
            return std::holds_alternative<TemperatureControlCmd>(command_variant_);
        }

        bool ControlCommand::isStreamControlCmd() const {
            return std::holds_alternative<StreamControlCmd>(command_variant_);
        }

        bool ControlCommand::isUserProfileCmd() const {
            return std::holds_alternative<UserProfileCmd>(command_variant_);
        }

        bool ControlCommand::isDeviceUnregisterCmd() const {
            return std::holds_alternative<DeviceUnregisterCmd>(command_variant_);
        }

// 转换为Protobuf
        device::control::ControlCommand ControlCommand::toProto() const {
            device::control::ControlCommand proto;

            // 根据变体类型设置对应的oneof字段
            if (isNewJobCmd()) {
                auto cmd = std::get<NewJobCmd>(command_variant_);
                *proto.mutable_new_job() = cmd.toProto();
            } else if (isNewLocalJobCmd()) {
                auto cmd = std::get<NewLocalJobCmd>(command_variant_);
                *proto.mutable_new_local_job() = cmd.toProto();
            } else if (isLightControlCmd()) {
                auto cmd = std::get<LightControlCmd>(command_variant_);
                *proto.mutable_light_control() = cmd.toProto();
            } else if (isTemperatureControlCmd()) {
                auto cmd = std::get<TemperatureControlCmd>(command_variant_);
                *proto.mutable_temperature_control() = cmd.toProto();
            } else if (isStreamControlCmd()) {
                auto cmd = std::get<StreamControlCmd>(command_variant_);
                *proto.mutable_stream_control() = cmd.toProto();
            } else if (isUserProfileCmd()) {
                auto cmd = std::get<UserProfileCmd>(command_variant_);
                *proto.mutable_user_profile() = cmd.toProto();
            } else if (isDeviceUnregisterCmd()) {
                auto cmd = std::get<DeviceUnregisterCmd>(command_variant_);
                *proto.mutable_device_unregister() = cmd.toProto();
            }

            return proto;
        }

// 序列化为二进制数据
        std::vector<uint8_t> ControlCommand::serializeToBytes() const {
            auto proto = toProto();
            std::string serialized = proto.SerializeAsString();
            return std::vector<uint8_t>(serialized.begin(), serialized.end());
        }

// 从二进制数据反序列化
        bool ControlCommand::parseFromBytes(const std::vector<uint8_t>& data) {
            device::control::ControlCommand proto;
            if (proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
                auto cmd = fromProto(proto);
                if (cmd) {
                    *this = std::move(*cmd);
                    return true;
                }
            }
            return false;
        }

// 序列化为JSON字符串
        std::string ControlCommand::serializeToJson() const {
            auto proto = toProto();
            std::string json_str;
            google::protobuf::util::JsonPrintOptions options;
            options.always_print_primitive_fields = true;

            // 检查是否有 preserve_proto_field_names 成员（处理Protobuf版本差异）
            // 注释掉可能导致编译错误的选项
            // options.preserve_proto_field_names = true;

            auto status = google::protobuf::util::MessageToJsonString(proto, &json_str, options);
            if (!status.ok()) {
                return "";
            }
            return json_str;
        }

// 从JSON字符串解析
        bool ControlCommand::parseFromJson(const std::string& json_str) {
            device::control::ControlCommand proto;
            google::protobuf::util::JsonParseOptions options;
            options.ignore_unknown_fields = true;

            auto status = google::protobuf::util::JsonStringToMessage(json_str, &proto, options);
            if (!status.ok()) {
                return false;
            }

            auto cmd = fromProto(proto);
            if (!cmd) {
                return false;
            }

            *this = std::move(*cmd);
            return true;
        }

// 验证命令
        bool ControlCommand::validate() const {
            return std::visit([](auto&& arg) -> bool {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, NewJobCmd>) {
                    return arg.validate();
                } else if constexpr (std::is_same_v<T, NewLocalJobCmd>) {
                    return arg.validate();
                } else if constexpr (std::is_same_v<T, LightControlCmd>) {
                    return arg.isValid();
                } else if constexpr (std::is_same_v<T, TemperatureControlCmd>) {
                    return arg.validate();
                } else if constexpr (std::is_same_v<T, StreamControlCmd>) {
                    return arg.isValid();
                } else if constexpr (std::is_same_v<T, UserProfileCmd>) {
                    return arg.validate();
                } else if constexpr (std::is_same_v<T, DeviceUnregisterCmd>) {
                    return arg.validate();
                }
                return false;
            }, command_variant_);
        }

// 转换为可读字符串
        std::string ControlCommand::toString() const {
            std::stringstream ss;
            ss << "ControlCommand{type=" << getCommandType()
               << ", timestamp=" << timestamp_
               << ", command_id=" << command_id_
               << ", device_id=" << device_id_ << "}";
            return ss.str();
        }

// 获取命令描述
        std::string ControlCommand::getDescription() const {
            std::stringstream ss;
            ss << "Command: " << getCommandType();

            // 添加命令特定信息
            if (isNewJobCmd()) {
                auto cmd = std::get<NewJobCmd>(command_variant_);
                ss << " (Jobs: " << cmd.jobCount() << ")";
            } else if (isNewLocalJobCmd()) {
                auto cmd = std::get<NewLocalJobCmd>(command_variant_);
                ss << " (JobID: " << cmd.args.job_id << ")";
            } else if (isTemperatureControlCmd()) {
                auto cmd = std::get<TemperatureControlCmd>(command_variant_);
                ss << " [Platform: " << cmd.platform
                   << "°C, Nozzle: " << cmd.right_nozzle << "/" << cmd.left_nozzle
                   << "°C]";
            } else if (isLightControlCmd()) {
                auto cmd = std::get<LightControlCmd>(command_variant_);
                ss << " [" << cmd.toString() << "]";
            } else if (isStreamControlCmd()) {
                auto cmd = std::get<StreamControlCmd>(command_variant_);
                ss << " [" << cmd.toString() << "]";
            } else if (isUserProfileCmd()) {
                auto cmd = std::get<UserProfileCmd>(command_variant_);
                ss << " [User: " << cmd.name << "]";
            } else if (isDeviceUnregisterCmd()) {
                ss << " [Device Unregister]";
            }

            return ss.str();
        }

// ============================================================================
// ControlCommandFactory 实现
// ============================================================================

        void ControlCommandFactory::registerCommandHandler(
                const std::string& cmd_type,
                CommandParser parser,
                CommandBuilder builder) {

            handlers_[cmd_type] = std::make_pair(parser, builder);
        }

        std::unique_ptr<ControlCommand> ControlCommandFactory::createCommand(
                const std::string& cmd_type, const std::string& args_json) {

            auto it = handlers_.find(cmd_type);
            if (it != handlers_.end()) {
                return it->second.first(args_json);
            }

            // 默认使用ControlCommand的工厂方法
            return ControlCommand::createFromString(cmd_type, args_json);
        }

        std::vector<uint8_t> ControlCommandFactory::serializeCommand(const ControlCommand& command) {
            auto it = handlers_.find(command.getCommandType());
            if (it != handlers_.end()) {
                auto proto = it->second.second(command);
                std::string serialized = proto.SerializeAsString();
                return std::vector<uint8_t>(serialized.begin(), serialized.end());
            }

            // 默认序列化
            return command.serializeToBytes();
        }

        std::unique_ptr<ControlCommand> ControlCommandFactory::deserializeCommand(
                const std::vector<uint8_t>& data) {

            device::control::ControlCommand proto;
            if (proto.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
                return ControlCommand::fromProto(proto);
            }
            return nullptr;
        }

        std::vector<std::string> ControlCommandFactory::getSupportedCommands() const {
            std::vector<std::string> commands;
            commands.reserve(handlers_.size());
            for (const auto& pair : handlers_) {
                commands.push_back(pair.first);
            }
            return commands;
        }

    } // namespace control
} // namespace swan