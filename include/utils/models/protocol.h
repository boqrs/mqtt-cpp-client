//
// Created by wave on 2026/1/20.
//
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

namespace swan {
namespace protocol {

// ==================== 基础数据结构 ====================

// 材料站槽位信息
struct SlotInfos {
    bool has_filament = false;
    std::string material_color;
    std::string material_name;
    uint32_t slot_id = 0;

    // JSON转换
    nlohmann::json toJson() const {
        nlohmann::json json;
        json["hasFilament"] = has_filament;
        json["materialColor"] = material_color;
        json["materialName"] = material_name;
        json["slotId"] = slot_id;
        return json;
    }

    static SlotInfos fromJson(const nlohmann::json& json) {
        SlotInfos info;
        info.has_filament = json.value("hasFilament", false);
        info.material_color = json.value("materialColor", "");
        info.material_name = json.value("materialName", "");
        info.slot_id = json.value("slotId", 0);
        return info;
    }
};

// 材料站信息
struct MatlStationInfo {
    uint32_t current_load_slot = 0;
    uint32_t current_slot = 0;
    uint32_t slot_cnt = 0;
    std::vector<SlotInfos> slot_infos;
    uint32_t state_action = 0;
    uint32_t state_step = 0;

    // JSON转换
    nlohmann::json toJson() const {
        nlohmann::json json;
        json["currentLoadSlot"] = current_load_slot;
        json["currentSlot"] = current_slot;
        json["slotCnt"] = slot_cnt;
        json["stateAction"] = state_action;
        json["stateStep"] = state_step;

        if (!slot_infos.empty()) {
            nlohmann::json slot_array = nlohmann::json::array();
            for (const auto& slot : slot_infos) {
                slot_array.push_back(slot.toJson());
            }
            json["slotInfos"] = slot_array;
        }

        return json;
    }

    static MatlStationInfo fromJson(const nlohmann::json& json) {
        MatlStationInfo info;
        info.current_load_slot = json.value("currentLoadSlot", 0);
        info.current_slot = json.value("currentSlot", 0);
        info.slot_cnt = json.value("slotCnt", 0);
        info.state_action = json.value("stateAction", 0);
        info.state_step = json.value("stateStep", 0);

        if (json.contains("slotInfos") && json["slotInfos"].is_array()) {
            for (const auto& slot_json : json["slotInfos"]) {
                info.slot_infos.push_back(SlotInfos::fromJson(slot_json));
            }
        }

        return info;
    }
};

// 设备状态数据
struct DeviceStateData {
    // 设备基本信息
    std::string device_id;
    std::string sn;
    std::string status;
    std::string ip_address;
    std::string mac;
    std::string printer_name;
    std::string firmware_version;

    // 打印信息
    uint32_t progress = 0;
    std::string job_id;
    std::string file_name;
    std::string file_path;
    uint32_t print_layer = 0;
    uint32_t target_layer = 0;
    uint32_t duration = 0;
    uint32_t actual_duration = 0;
    uint32_t estimate_time = 0;

    // 温度信息
    uint32_t left_temperature = 0;
    uint32_t left_target_temperature = 0;
    uint32_t right_temperature = 0;
    uint32_t right_target_temperature = 0;
    uint32_t platform_cur_temperature = 0;
    uint32_t platform_target_temperature = 0;
    int32_t chamber_temp = -90;
    uint32_t chamber_target_temp = 0;

    // 耗材信息
    uint32_t left_filament = 0;
    std::string left_filament_type;
    uint32_t right_filament = 0;
    std::string right_filament_type;
    double cumulative_filament = 0.0;
    uint32_t estimate_length_left = 0;
    uint32_t estimate_length_right = 0;

    // 材料站信息
    MatlStationInfo matl_station_info;

    // 喷嘴信息
    uint32_t nozzle_count = 0;
    std::string nozzle_model;
    uint32_t nozzle_style = 0;
    std::vector<uint32_t> nozzle_temps;
    std::vector<uint32_t> nozzle_target_temps;

    // 其他设备状态
    uint32_t camera = 0;
    uint32_t lidar = 0;
    std::string stream;
    std::string hls_stream;
    std::string light;
    std::string door;
    std::string external;
    std::string internal;
    uint32_t chamber_fan = 0;
    uint32_t cooling_fan = 0;
    std::string delay_close;
    uint32_t delay_time = 0;

    // 其他信息
    std::string error_code;
    uint32_t current_speed = 0;
    uint32_t entirety_speed = 0;
    double cumulative_print_time = 0;
    uint32_t filling_amount = 0;
    std::string flash_register_code;
    std::string polar_register_code;
    std::string location;
    std::string language;
    std::string measure;
    uint32_t model_weight = 0;
    std::string pid;
    std::string thumbnail_path;
    uint32_t tvoc = 0;
    uint32_t z_axis_compensation = 0;
    double remain_memory = 0.0;

    // JSON转换
    nlohmann::json toJson() const {
        nlohmann::json json;

        // 设备基本信息
        json["deviceID"] = device_id;
        json["sn"] = sn;
        json["status"] = status;
        json["ipAddress"] = ip_address;
        json["mac"] = mac;
        json["printerName"] = printer_name;
        json["firmwareVersion"] = firmware_version;

        // 打印信息
        json["progress"] = progress;
        json["jobID"] = job_id;
        json["fileName"] = file_name;
        json["filePath"] = file_path;
        json["printLayer"] = print_layer;
        json["targetLayer"] = target_layer;
        json["duration"] = duration;
        json["actualDuration"] = actual_duration;
        json["estimateTime"] = estimate_time;

        // 温度信息
        json["leftTemperature"] = left_temperature;
        json["leftTargetTemperature"] = left_target_temperature;
        json["rightTemperature"] = right_temperature;
        json["rightTargetTemperature"] = right_target_temperature;
        json["platformCurTemperature"] = platform_cur_temperature;
        json["platformTargetTemperature"] = platform_target_temperature;
        json["chamberTemp"] = chamber_temp;
        json["chamberTargetTemp"] = chamber_target_temp;

        // 耗材信息
        json["leftFilament"] = left_filament;
        json["leftFilamentType"] = left_filament_type;
        json["rightFilament"] = right_filament;
        json["rightFilamentType"] = right_filament_type;
        json["cumulativeFilament"] = cumulative_filament;
        json["estimateLengthLeft"] = estimate_length_left;
        json["estimateLengthRight"] = estimate_length_right;

        // 材料站信息
        json["matlStationInfo"] = matl_station_info.toJson();

        // 喷嘴信息
        json["nozzleCount"] = nozzle_count;
        json["nozzleModel"] = nozzle_model;
        json["nozzleStyle"] = nozzle_style;

        if (!nozzle_temps.empty()) {
            json["nozzleTemps"] = nozzle_temps;
        }

        if (!nozzle_target_temps.empty()) {
            json["nozzleTargetTemps"] = nozzle_target_temps;
        }

        // 其他设备状态
        json["camera"] = camera;
        json["lidar"] = lidar;
        json["stream"] = stream;
        json["hlsStream"] = hls_stream;
        json["light"] = light;
        json["door"] = door;
        json["external"] = external;
        json["internal"] = internal;
        json["chamberFan"] = chamber_fan;
        json["coolingFan"] = cooling_fan;
        json["delayClose"] = delay_close;
        json["delayTime"] = delay_time;

        // 其他信息
        json["errorCode"] = error_code;
        json["currentSpeed"] = current_speed;
        json["entiretySpeed"] = entirety_speed;
        json["cumulativePrintTime"] = cumulative_print_time;
        json["fillingAmount"] = filling_amount;
        json["flashRegisterCode"] = flash_register_code;
        json["polarRegisterCode"] = polar_register_code;
        json["location"] = location;
        json["language"] = language;
        json["measure"] = measure;
        json["modelWeight"] = model_weight;
        json["pid"] = pid;
        json["thumbnailPath"] = thumbnail_path;
        json["tvoc"] = tvoc;
        json["zAxisCompensation"] = z_axis_compensation;
        json["remainMemory"] = remain_memory;

        return json;
    }

    static DeviceStateData fromJson(const nlohmann::json& json) {
        DeviceStateData data;

        // 设备基本信息
        data.device_id = json.value("deviceID", "");
        data.sn = json.value("sn", "");
        data.status = json.value("status", "");
        data.ip_address = json.value("ipAddress", "");
        data.mac = json.value("mac", "");
        data.printer_name = json.value("printerName", "");
        data.firmware_version = json.value("firmwareVersion", "");

        // 打印信息
        data.progress = json.value("progress", 0);
        data.job_id = json.value("jobID", "");
        data.file_name = json.value("fileName", "");
        data.file_path = json.value("filePath", "");
        data.print_layer = json.value("printLayer", 0);
        data.target_layer = json.value("targetLayer", 0);
        data.duration = json.value("duration", 0);
        data.actual_duration = json.value("actualDuration", 0);
        data.estimate_time = json.value("estimateTime", 0);

        // 温度信息
        data.left_temperature = json.value("leftTemperature", 0);
        data.left_target_temperature = json.value("leftTargetTemperature", 0);
        data.right_temperature = json.value("rightTemperature", 0);
        data.right_target_temperature = json.value("rightTargetTemperature", 0);
        data.platform_cur_temperature = json.value("platformCurTemperature", 0);
        data.platform_target_temperature = json.value("platformTargetTemperature", 0);
        data.chamber_temp = json.value("chamberTemp", -90);
        data.chamber_target_temp = json.value("chamberTargetTemp", 0);

        // 耗材信息
        data.left_filament = json.value("leftFilament", 0);
        data.left_filament_type = json.value("leftFilamentType", "");
        data.right_filament = json.value("rightFilament", 0);
        data.right_filament_type = json.value("rightFilamentType", "");
        data.cumulative_filament = json.value("cumulativeFilament", 0.0);
        data.estimate_length_left = json.value("estimateLengthLeft", 0);
        data.estimate_length_right = json.value("estimateLengthRight", 0);

        // 材料站信息
        if (json.contains("matlStationInfo")) {
            data.matl_station_info = MatlStationInfo::fromJson(json["matlStationInfo"]);
        }

        // 喷嘴信息
        data.nozzle_count = json.value("nozzleCount", 0);
        data.nozzle_model = json.value("nozzleModel", "");
        data.nozzle_style = json.value("nozzleStyle", 0);

        if (json.contains("nozzleTemps") && json["nozzleTemps"].is_array()) {
            for (const auto& temp : json["nozzleTemps"]) {
                data.nozzle_temps.push_back(temp);
            }
        }

        if (json.contains("nozzleTargetTemps") && json["nozzleTargetTemps"].is_array()) {
            for (const auto& temp : json["nozzleTargetTemps"]) {
                data.nozzle_target_temps.push_back(temp);
            }
        }

        // 其他设备状态
        data.camera = json.value("camera", 0);
        data.lidar = json.value("lidar", 0);
        data.stream = json.value("stream", "");
        data.hls_stream = json.value("hlsStream", "");
        data.light = json.value("light", "");
        data.door = json.value("door", "");
        data.external = json.value("external", "");
        data.internal = json.value("internal", "");
        data.chamber_fan = json.value("chamberFan", 0);
        data.cooling_fan = json.value("coolingFan", 0);
        data.delay_close = json.value("delayClose", "");
        data.delay_time = json.value("delayTime", 0);

        // 其他信息
        data.error_code = json.value("errorCode", "");
        data.current_speed = json.value("currentSpeed", 0);
        data.entirety_speed = json.value("entiretySpeed", 0);
        data.cumulative_print_time = json.value("cumulativePrintTime", 0);
        data.filling_amount = json.value("fillingAmount", 0);
        data.flash_register_code = json.value("flashRegisterCode", "");
        data.polar_register_code = json.value("polarRegisterCode", "");
        data.location = json.value("location", "");
        data.language = json.value("language", "");
        data.measure = json.value("measure", "");
        data.model_weight = json.value("modelWeight", 0);
        data.pid = json.value("pid", "");
        data.thumbnail_path = json.value("thumbnailPath", "");
        data.tvoc = json.value("tvoc", 0);
        data.z_axis_compensation = json.value("zAxisCompensation", 0);
        data.remain_memory = json.value("remainMemory", 0.0);

        return data;
    }

    // 验证数据
    bool validate() const {
        return !device_id.empty() && !sn.empty() && !status.empty();
    }

    // 转换为字符串
    std::string toString() const {
        return "DeviceStateData{device_id=" + device_id +
               ", sn=" + sn +
               ", status=" + status +
               ", progress=" + std::to_string(progress) + "%}";
    }
};

// ==================== 下行命令数据结构 ====================

// 材质信息
struct MaterialInfo {
    int32_t slot = 0;
    int32_t t_num = 0;
    std::string s_rgb;
    std::string t_rgb;
    std::string material_type;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["slot"] = slot;
        json["t_num"] = t_num;
        json["s_rgb"] = s_rgb;
        json["t_rgb"] = t_rgb;
        json["material_type"] = material_type;
        return json;
    }

    static MaterialInfo fromJson(const nlohmann::json& json) {
        MaterialInfo info;
        info.slot = json.value("slot", 0);
        info.t_num = json.value("t_num", 0);
        info.s_rgb = json.value("s_rgb", "");
        info.t_rgb = json.value("t_rgb", "");
        info.material_type = json.value("material_type", "");
        return info;
    }
};

// 远程作业参数
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
    std::vector<MaterialInfo> ms;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["filename"] = filename;
        json["file_md5"] = file_md5;
        json["filepath"] = filepath;
        json["thumb_path"] = thumb_path;
        json["print_now"] = print_now;
        json["leveling"] = leveling;
        json["flow_calibration"] = flow_calibration;
        json["use_ms"] = use_ms;
        json["t_count"] = t_count;

        if (!ms.empty()) {
            nlohmann::json ms_array = nlohmann::json::array();
            for (const auto& material : ms) {
                ms_array.push_back(material.toJson());
            }
            json["ms"] = ms_array;
        }

        return json;
    }
};

// 作业信息
struct JobInfo {
    std::string sn;
    std::string uuid;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["sn"] = sn;
        json["uuid"] = uuid;
        return json;
    }
};

// 新远程作业命令
struct NewJobCmd {
    std::vector<JobInfo> job_info;
    NewJobArgs args;

    nlohmann::json toJson() const {
        nlohmann::json json;

        if (!job_info.empty()) {
            nlohmann::json job_array = nlohmann::json::array();
            for (const auto& job : job_info) {
                job_array.push_back(job.toJson());
            }
            json["job_info"] = job_array;
        }

        json["args"] = args.toJson();
        return json;
    }
};

// 本地作业参数
struct NewLocalJobArgs {
    std::string job_id;
    std::string file_name;
    bool print_now = false;
    bool leveling = false;
    bool flow_calibration = false;
    bool use_ms = false;
    int32_t t_count = 0;
    std::vector<MaterialInfo> ms;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["job_id"] = job_id;
        json["file_name"] = file_name;
        json["print_now"] = print_now;
        json["leveling"] = leveling;
        json["flow_calibration"] = flow_calibration;
        json["use_ms"] = use_ms;
        json["t_count"] = t_count;

        if (!ms.empty()) {
            nlohmann::json ms_array = nlohmann::json::array();
            for (const auto& material : ms) {
                ms_array.push_back(material.toJson());
            }
            json["ms"] = ms_array;
        }

        return json;
    }
};

// 新本地作业命令
struct NewLocalJobCmd {
    NewLocalJobArgs args;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["args"] = args.toJson();
        return json;
    }
};

// 灯光控制命令
struct LightControlCmd {
    enum class LightStatus {
        UNKNOWN = 0,
        OPEN = 1,
        CLOSE = 2
    } status = LightStatus::UNKNOWN;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["status"] = static_cast<int>(status);
        return json;
    }
};

// 温度控制命令
struct TemperatureControlCmd {
    int32_t platform = 0;
    int32_t right_nozzle = 0;
    int32_t left_nozzle = 0;
    int32_t chamber = 0;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["platform"] = platform;
        json["right_nozzle"] = right_nozzle;
        json["left_nozzle"] = left_nozzle;
        json["chamber"] = chamber;
        return json;
    }
};

// 流控制命令
struct StreamControlCmd {
    enum class StreamAction {
        UNKNOWN_ACTION = 0,
        OPEN = 1,
        CLOSE = 2
    } action = StreamAction::UNKNOWN_ACTION;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["action"] = static_cast<int>(action);
        return json;
    }
};

// 用户资料命令
struct UserProfileCmd {
    std::string avatar;
    std::string name;

    nlohmann::json toJson() const {
        nlohmann::json json;
        json["avatar"] = avatar;
        json["name"] = name;
        return json;
    }
};

// 设备注销命令
struct DeviceUnregisterCmd {
    nlohmann::json toJson() const {
        return nlohmann::json::object();  // 空对象
    }
};

// 控制命令联合体
using ControlCommand = std::variant<
    NewJobCmd,
    NewLocalJobCmd,
    LightControlCmd,
    TemperatureControlCmd,
    StreamControlCmd,
    UserProfileCmd,
    DeviceUnregisterCmd
>;

// ==================== 统一消息格式 ====================

// 统一消息类型
enum class MessageType {
    DEVICE_STATE = 0,
    DEVICE_COMMAND,
    UNKNOWN
};

// 统一消息
struct UnifiedMessage {
    MessageType message_type = MessageType::UNKNOWN;
    std::string name;  // 对于命令消息：指令名称
    std::string action_type;  // 对于状态消息："device_status"，对于命令消息：具体指令类型

    // 数据负载
    std::variant<DeviceStateData, ControlCommand> data;

    // 可选字段
    std::string timestamp;
    std::string request_id;

    // 转换为JSON
    nlohmann::json toJson() const {
        nlohmann::json json;

        // 设置消息类型
        switch (message_type) {
            case MessageType::DEVICE_STATE:
                json["messageType"] = "device_state";
                break;
            case MessageType::DEVICE_COMMAND:
                json["messageType"] = "device_cmd";
                break;
            default:
                json["messageType"] = "unknown";
                break;
        }

        // 设置名称（命令消息才有）
        if (!name.empty()) {
            json["name"] = name;
        }

        // 设置payload
        nlohmann::json payload;
        payload["action_type"] = action_type;

        // 设置数据
        if (std::holds_alternative<DeviceStateData>(data)) {
            const auto& state_data = std::get<DeviceStateData>(data);
            payload["data"] = state_data.toJson();
        } else if (std::holds_alternative<ControlCommand>(data)) {
            const auto& cmd = std::get<ControlCommand>(data);

            // 根据具体命令类型转换为JSON
            std::visit([&payload](auto&& arg) {
                payload["data"] = arg.toJson();
            }, cmd);
        }

        json["payload"] = payload;

        // 可选字段
        if (!timestamp.empty()) {
            json["timestamp"] = timestamp;
        }

        if (!request_id.empty()) {
            json["requestId"] = request_id;
        }

        return json;
    }

    // 从JSON解析
    static UnifiedMessage fromJson(const nlohmann::json& json) {
        UnifiedMessage msg;

        // 解析消息类型
        std::string message_type_str = json.value("messageType", "");
        if (message_type_str == "device_state") {
            msg.message_type = MessageType::DEVICE_STATE;
        } else if (message_type_str == "device_cmd") {
            msg.message_type = MessageType::DEVICE_COMMAND;
        } else {
            msg.message_type = MessageType::UNKNOWN;
        }

        // 解析名称
        msg.name = json.value("name", "");

        // 解析payload
        if (json.contains("payload")) {
            const auto& payload = json["payload"];
            msg.action_type = payload.value("action_type", "");

            if (payload.contains("data")) {
                if (msg.message_type == MessageType::DEVICE_STATE) {
                    // 解析设备状态数据
                    msg.data = DeviceStateData::fromJson(payload["data"]);
                } else if (msg.message_type == MessageType::DEVICE_COMMAND) {
                    // 解析控制命令（简化处理，根据action_type判断）
                    if (msg.action_type == "new_job") {
                        // 需要实现具体的解析逻辑
                    }
                    // 其他命令类型的解析...
                }
            }
        }

        // 解析可选字段
        msg.timestamp = json.value("timestamp", "");
        msg.request_id = json.value("requestId", "");

        return msg;
    }

    // 验证消息
    bool validate() const {
        if (message_type == MessageType::UNKNOWN) {
            return false;
        }

        if (message_type == MessageType::DEVICE_STATE) {
            if (!std::holds_alternative<DeviceStateData>(data)) {
                return false;
            }
            const auto& state_data = std::get<DeviceStateData>(data);
            return state_data.validate();
        } else if (message_type == MessageType::DEVICE_COMMAND) {
            return !name.empty() && !action_type.empty();
        }

        return false;
    }

    // 转换为字符串
    std::string toString() const {
        std::string result = "UnifiedMessage{";

        switch (message_type) {
            case MessageType::DEVICE_STATE:
                result += "type=device_state";
                if (std::holds_alternative<DeviceStateData>(data)) {
                    const auto& state_data = std::get<DeviceStateData>(data);
                    result += ", device_id=" + state_data.device_id;
                    result += ", status=" + state_data.status;
                }
                break;
            case MessageType::DEVICE_COMMAND:
                result += "type=device_cmd";
                result += ", name=" + name;
                result += ", action_type=" + action_type;
                break;
            default:
                result += "type=unknown";
                break;
        }

        result += "}";
        return result;
    }

     // 检查是否是设备状态消息
    bool isDeviceStateMessage() const {
        return message_type == MessageType::DEVICE_STATE &&
               std::holds_alternative<DeviceStateData>(data);
    }

    // 检查是否是设备命令消息
    bool isDeviceCommandMessage() const {
        return message_type == MessageType::DEVICE_COMMAND &&
               std::holds_alternative<ControlCommand>(data);
    }

    // 获取设备状态数据（如果是状态消息）
   const DeviceStateData* getDeviceStateData() const {
        if (isDeviceStateMessage()) {
            return &std::get<DeviceStateData>(data);
        }
        return nullptr;
    }

    DeviceStateData* getMutableDeviceStateData() {
        if (isDeviceStateMessage()) {
            return &std::get<DeviceStateData>(data);
        }
        return nullptr;
    }

    // 获取控制命令（如果是命令消息）
    const ControlCommand* getControlCommand() const {
        if (isDeviceCommandMessage()) {
            return &std::get<ControlCommand>(data);
        }
        return nullptr;
    }

    ControlCommand* getMutableControlCommand() {
        if (isDeviceCommandMessage()) {
            return &std::get<ControlCommand>(data);
        }
        return nullptr;
    }

    // 获取设备ID（如果是状态消息）
    std::string getDeviceId() const {
        if (auto state_data = getDeviceStateData()) {
            return state_data->device_id;
        }
        return "";
    }

    // 获取序列号（如果是状态消息）
    std::string getSerialNumber() const {
        if (auto state_data = getDeviceStateData()) {
            return state_data->sn;
        }
        return "";
    }

    // 获取设备状态（如果是状态消息）
    std::string getDeviceStatus() const {
        if (auto state_data = getDeviceStateData()) {
            return state_data->status;
        }
        return "";
    }

    // 获取打印进度（如果是状态消息）
    uint32_t getProgress() const {
        if (auto state_data = getDeviceStateData()) {
            return state_data->progress;
        }
        return 0;
    }

    // 验证设备状态数据
    bool validateDeviceState() const {
        if (auto state_data = getDeviceStateData()) {
            return state_data->validate();
        }
        return false;
    }
};

// ==================== 消息工厂类 ====================

class MessageFactory {
public:
    // 创建设备状态消息
    static UnifiedMessage createStateMessage(const DeviceStateData& state_data,
                                            const std::string& action_type = "device_status") {
        UnifiedMessage msg;
        msg.message_type = MessageType::DEVICE_STATE;
        msg.action_type = action_type;
        msg.data = state_data;
        return msg;
    }

    // 创建命令消息
    template<typename T>
    static UnifiedMessage createCommandMessage(const T& command,
                                              const std::string& name,
                                              const std::string& action_type) {
        UnifiedMessage msg;
        msg.message_type = MessageType::DEVICE_COMMAND;
        msg.name = name;
        msg.action_type = action_type;
        msg.data = command;
        return msg;
    }

    // 创建灯光控制命令
    static UnifiedMessage createLightControlCommand(LightControlCmd::LightStatus status,
                                                   const std::string& device_id = "") {
        LightControlCmd cmd;
        cmd.status = status;

        UnifiedMessage msg;
        msg.message_type = MessageType::DEVICE_COMMAND;
        msg.name = "light_control";
        msg.action_type = "light_control";
        msg.data = cmd;

        if (!device_id.empty()) {
            // 可以在这里添加设备ID到消息中
        }

        return msg;
    }

    // 创建温度控制命令
    static UnifiedMessage createTemperatureControlCommand(int32_t platform_temp,
                                                         int32_t right_nozzle_temp,
                                                         int32_t left_nozzle_temp,
                                                         int32_t chamber_temp,
                                                         const std::string& device_id= "") {
        TemperatureControlCmd cmd;
        cmd.platform = platform_temp;
        cmd.right_nozzle = right_nozzle_temp;
        cmd.left_nozzle = left_nozzle_temp;
        cmd.chamber = chamber_temp;

        UnifiedMessage msg;
        msg.message_type = MessageType::DEVICE_COMMAND;
        msg.name = "temperature_control";
        msg.action_type = "temperature_control";
        msg.data = cmd;

        return msg;
    }
};

} // namespace protocol
} // namespace swan