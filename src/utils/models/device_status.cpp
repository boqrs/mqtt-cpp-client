//
// Created by wave on 2026/1/12.
//
#include "utils/models/device_status.h"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace swan {
    namespace models {

// ============================================================================
// MaterialSlot 实现
// ============================================================================

        MaterialSlot MaterialSlot::fromProto(const SomeMessage_Slot_infos& proto) {
            MaterialSlot slot;
            slot.has_filament = proto.has_filament();
            slot.material_color = proto.material_color();
            slot.material_name = proto.material_name();
            slot.slot_id = proto.slot_id();
            return slot;
        }

        void MaterialSlot::toProto(SomeMessage_Slot_infos* proto) const {
            if (proto) {
                proto->set_has_filament(has_filament);
                proto->set_material_color(material_color);
                proto->set_material_name(material_name);
                proto->set_slot_id(slot_id);
            }
        }

// ============================================================================
// MaterialStation 实现
// ============================================================================

        MaterialStation MaterialStation::fromProto(const SomeMessage_Matl_station_info& proto) {
            MaterialStation station;
            station.current_load_slot = proto.current_load_slot();
            station.current_slot = proto.current_slot();
            station.slot_cnt = proto.slot_cnt();
            station.state_action = proto.state_action();
            station.state_step = proto.state_step();

            // 处理 slot_infos
            station.slot_infos.clear();
            for (int i = 0; i < proto.slot_infos_size(); ++i) {
                station.slot_infos.push_back(MaterialSlot::fromProto(proto.slot_infos(i)));
            }
            return station;
        }

        void MaterialStation::toProto(SomeMessage_Matl_station_info* proto) const {
            if (proto) {
                proto->set_current_load_slot(current_load_slot);
                proto->set_current_slot(current_slot);
                proto->set_slot_cnt(slot_cnt);
                proto->set_state_action(state_action);
                proto->set_state_step(state_step);

                // 处理 slot_infos
                proto->clear_slot_infos();
                for (const auto& slot : slot_infos) {
                    slot.toProto(proto->add_slot_infos());
                }
            }
        }

        const MaterialSlot* MaterialStation::getSlotInfo(uint32_t slot_id) const {
            for (const auto& slot : slot_infos) {
                if (slot.slot_id == slot_id) {
                    return &slot;
                }
            }
            return nullptr;
        }

        bool MaterialStation::hasAvailableMaterial() const {
            for (const auto& slot : slot_infos) {
                if (slot.has_filament) {
                    return true;
                }
            }
            return false;
        }

// ============================================================================
// DeviceStatusData 实现
// ============================================================================

        DeviceStatusData::DeviceStatusData() :
                camera(0), chamber_fan(0), cooling_fan(0), current_speed(0),
                entirety_speed(0), estimate_length_left(0), estimate_length_right(0),
                delay_time(0), lidar(0), model_weight(0),
                nozzle_count(0), nozzle_style(0), remain_memory(0.0),
                tvoc(0), z_axis_compensation(0), filling_amount(0) {
            // 初始化 TemperatureInfo 结构体
            temperatures.chamber_temp = 0;
            temperatures.chamber_target_temp = 0;
            temperatures.left_temperature = 0;
            temperatures.left_target_temperature = 0;
            temperatures.right_temperature = 0;
            temperatures.right_target_temperature = 0;
            temperatures.platform_cur_temperature = 0;
            temperatures.platform_target_temperature = 0;

            // 初始化 PrintProgress 结构体
            progress.progress = 0;
            progress.print_layer = 0;
            progress.target_layer = 0;
            progress.estimate_time = 0;
            progress.actual_duration = 0;
            progress.duration = 0;
            progress.cumulative_print_time = 0;

            // 初始化 MaterialInfo 结构体
            materials.left_filament = 0;
            materials.right_filament = 0;
            materials.cumulative_filament = 0.0;
        }

        DeviceStatusData DeviceStatusData::fromProto(const SomeMessage_Data& proto) {
            DeviceStatusData data;

            // 转换温度信息
            data.temperatures.chamber_temp = proto.chamber_temp();
            data.temperatures.chamber_target_temp = proto.chamber_target_temp();
            data.temperatures.left_temperature = proto.left_temperature();
            data.temperatures.left_target_temperature = proto.left_target_temperature();
            data.temperatures.right_temperature = proto.right_temperature();
            data.temperatures.right_target_temperature = proto.right_target_temperature();
            data.temperatures.platform_cur_temperature = proto.platform_cur_temperature();
            data.temperatures.platform_target_temperature = proto.platform_target_temperature();

            // 转换喷嘴温度数组
            data.temperatures.nozzle_temps.clear();
            for (int i = 0; i < proto.nozzle_temps_size(); ++i) {
                data.temperatures.nozzle_temps.push_back(proto.nozzle_temps(i));
            }

            // 转换喷嘴目标温度数组
            data.temperatures.nozzle_target_temps.clear();
            for (int i = 0; i < proto.nozzle_target_temps_size(); ++i) {
                data.temperatures.nozzle_target_temps.push_back(proto.nozzle_target_temps(i));
            }

            // 转换打印进度
            data.progress.progress = proto.progress();
            data.progress.print_layer = proto.print_layer();
            data.progress.target_layer = proto.target_layer();
            data.progress.estimate_time = proto.estimate_time();
            data.progress.actual_duration = proto.actual_duration();
            data.progress.duration = proto.duration();
            data.progress.cumulative_print_time = proto.cumulative_print_time();
            data.progress.file_name = proto.file_name();
            data.progress.file_path = proto.file_path();

            // 转换材料信息
            data.materials.left_filament = proto.left_filament();
            data.materials.right_filament = proto.right_filament();
            data.materials.left_filament_type = proto.left_filament_type();
            data.materials.right_filament_type = proto.right_filament_type();
            data.materials.cumulative_filament = proto.cumulative_filament();

            // 转换材料站信息
            data.materials.material_station = MaterialStation::fromProto(proto.matl_station_info());

            // 转换设备信息
            data.device.device_id = proto.device_i_d();
            data.device.printer_name = proto.printer_name();
            data.device.sn = proto.sn();
            data.device.mac = proto.mac();
            data.device.ip_address = proto.ip_address();
            data.device.firmware_version = proto.firmware_version();
            data.device.status = proto.status();
            data.device.error_code = proto.error_code();

            // 转换其他字段
            data.camera = proto.camera();
            data.chamber_fan = proto.chamber_fan();
            data.cooling_fan = proto.cooling_fan();
            data.current_speed = proto.current_speed();
            data.entirety_speed = proto.entirety_speed();
            data.estimate_length_left = proto.estimate_length_left();
            data.estimate_length_right = proto.estimate_length_right();
            data.delay_close = proto.delay_close();
            data.delay_time = proto.delay_time();
            data.door = proto.door();
            data.external = proto.external();
            data.internal = proto.internal();
            data.job_id = proto.job_i_d();
            data.language = proto.language();
            data.lidar = proto.lidar();
            data.light = proto.light();
            data.location = proto.location();
            data.measure = proto.measure();  // 修正：measure 是字符串
            data.model_weight = proto.model_weight();
            data.nozzle_count = proto.nozzle_count();
            data.nozzle_model = proto.nozzle_model();
            data.nozzle_style = proto.nozzle_style();
            data.pid = proto.pid();
            data.polar_register_code = proto.polar_register_code();
            data.flash_register_code = proto.flash_register_code();
            data.remain_memory = proto.remain_memory();
            data.stream = proto.stream();
            data.hls_stream = proto.hls_stream();
            data.thumbnail_path = proto.thumbnail_path();
            data.tvoc = proto.tvoc();
            data.z_axis_compensation = proto.z_axis_compensation();
            data.filling_amount = proto.filling_amount();

            return data;
        }

        void DeviceStatusData::toProto(SomeMessage_Data* proto) const {
            if (!proto) return;

            // 设置温度信息
            proto->set_chamber_temp(temperatures.chamber_temp);
            proto->set_chamber_target_temp(temperatures.chamber_target_temp);
            proto->set_left_temperature(temperatures.left_temperature);
            proto->set_left_target_temperature(temperatures.left_target_temperature);
            proto->set_right_temperature(temperatures.right_temperature);
            proto->set_right_target_temperature(temperatures.right_target_temperature);
            proto->set_platform_cur_temperature(temperatures.platform_cur_temperature);
            proto->set_platform_target_temperature(temperatures.platform_target_temperature);

            // 设置喷嘴温度数组
            proto->clear_nozzle_temps();
            for (const auto& temp : temperatures.nozzle_temps) {
                proto->add_nozzle_temps(temp);
            }

            // 设置喷嘴目标温度数组
            proto->clear_nozzle_target_temps();
            for (const auto& temp : temperatures.nozzle_target_temps) {
                proto->add_nozzle_target_temps(temp);
            }

            // 设置打印进度
            proto->set_progress(progress.progress);
            proto->set_print_layer(progress.print_layer);
            proto->set_target_layer(progress.target_layer);
            proto->set_estimate_time(progress.estimate_time);
            proto->set_actual_duration(progress.actual_duration);
            proto->set_duration(progress.duration);
            proto->set_cumulative_print_time(progress.cumulative_print_time);
            proto->set_file_name(progress.file_name);
            proto->set_file_path(progress.file_path);

            // 设置材料信息
            proto->set_left_filament(materials.left_filament);
            proto->set_right_filament(materials.right_filament);
            proto->set_left_filament_type(materials.left_filament_type);
            proto->set_right_filament_type(materials.right_filament_type);
            proto->set_cumulative_filament(materials.cumulative_filament);

            // 设置材料站信息
            materials.material_station.toProto(proto->mutable_matl_station_info());

            // 设置设备信息
            proto->set_device_i_d(device.device_id);
            proto->set_printer_name(device.printer_name);
            proto->set_sn(device.sn);
            proto->set_mac(device.mac);
            proto->set_ip_address(device.ip_address);
            proto->set_firmware_version(device.firmware_version);
            proto->set_status(device.status);
            proto->set_error_code(device.error_code);

            // 设置其他字段
            proto->set_camera(camera);
            proto->set_chamber_fan(chamber_fan);
            proto->set_cooling_fan(cooling_fan);
            proto->set_current_speed(current_speed);
            proto->set_entirety_speed(entirety_speed);
            proto->set_estimate_length_left(estimate_length_left);
            proto->set_estimate_length_right(estimate_length_right);
            proto->set_delay_close(delay_close);
            proto->set_delay_time(delay_time);
            proto->set_door(door);
            proto->set_external(external);
            proto->set_internal(internal);
            proto->set_job_i_d(job_id);
            proto->set_language(language);
            proto->set_lidar(lidar);
            proto->set_light(light);
            proto->set_location(location);
            proto->set_measure(measure);  // 修正：measure 是字符串
            proto->set_model_weight(model_weight);
            proto->set_nozzle_count(nozzle_count);
            proto->set_nozzle_model(nozzle_model);
            proto->set_nozzle_style(nozzle_style);
            proto->set_pid(pid);
            proto->set_polar_register_code(polar_register_code);
            proto->set_flash_register_code(flash_register_code);
            proto->set_remain_memory(remain_memory);
            proto->set_stream(stream);
            proto->set_hls_stream(hls_stream);
            proto->set_thumbnail_path(thumbnail_path);
            proto->set_tvoc(tvoc);
            proto->set_z_axis_compensation(z_axis_compensation);
            proto->set_filling_amount(filling_amount);
        }

        bool DeviceStatusData::isPrinting() const {
            // 假设状态为 "printing"、"pause" 等表示正在打印
            return device.status.find("printing") != std::string::npos ||
                   device.status.find("pause") != std::string::npos ||
                   (progress.progress > 0 && progress.progress < 100);
        }

        bool DeviceStatusData::hasError() const {
            return !device.error_code.empty() || device.status.find("error") != std::string::npos;
        }

        double DeviceStatusData::getFilamentRemainingPercentage() const {
            if (materials.left_filament == 0 && materials.right_filament == 0) {
                return 0.0;
            }

            // 简单的百分比计算，假设初始长度为 1000
            const uint32_t initial_filament = 1000;
            double left_percent = (double) materials.left_filament / initial_filament * 100.0;
            double right_percent = (double) materials.right_filament / initial_filament * 100.0;

            // 返回剩余较多的那个百分比
            return std::max(left_percent, right_percent);
        }

        std::string DeviceStatusData::getFormattedRemainingTime() const {
            uint32_t remaining_seconds = progress.estimate_time - progress.actual_duration;
            if (remaining_seconds <= 0) return "0m";

            uint32_t hours = remaining_seconds / 3600;
            uint32_t minutes = (remaining_seconds % 3600) / 60;
            uint32_t seconds = remaining_seconds % 60;

            std::stringstream ss;
            if (hours > 0) {
                ss << hours << "h";
            }
            if (minutes > 0) {
                if (hours > 0) ss << " ";
                ss << minutes << "m";
            }
            if (seconds > 0 && hours == 0) {
                if (minutes > 0) ss << " ";
                ss << seconds << "s";
            }

            return ss.str();
        }

// ============================================================================
// DeviceStatus 实现
// ============================================================================

        DeviceStatus::DeviceStatus() : action_type_("") {}

        DeviceStatus::~DeviceStatus() = default;

        void DeviceStatus::setEventType(const std::string& event_type) {
            event_type_ = event_type;
        }

        const std::string& DeviceStatus::getEventType() const {
            return event_type_;
        }

        void DeviceStatus::setActionType(const std::string& action_type) {
            action_type_ = action_type;
        }

        const std::string& DeviceStatus::getActionType() const {
            return action_type_;
        }

        void DeviceStatus::setData(const DeviceStatusData& data) {
            data_ = data;
        }

        const DeviceStatusData& DeviceStatus::getData() const {
            return data_;
        }

        DeviceStatusData& DeviceStatus::getMutableData() {
            return data_;
        }

        DeviceStatus DeviceStatus::fromProto(const SomeMessage& proto) {
            DeviceStatus status;
            status.setEventType(proto.event_type());
            if (proto.has_payload()) {
                status.setActionType(proto.payload().action_type());
                status.setData(DeviceStatusData::fromProto(proto.payload().data()));
            }
            return status;
        }

        void DeviceStatus::toProto(SomeMessage* proto) const {
            if (proto) {
                proto->set_event_type(event_type_);
                auto* payload = proto->mutable_payload();
                payload->set_action_type(action_type_);
                data_.toProto(payload->mutable_data());
            }
        }

        std::string DeviceStatus::serializeToString() const {
            SomeMessage proto;
            toProto(&proto);
            return proto.SerializeAsString();
        }

        bool DeviceStatus::parseFromString(const std::string& data) {
            SomeMessage proto;
            if (!proto.ParseFromString(data)) {
                return false;
            }

            *this = fromProto(proto);
            return true;
        }

        bool DeviceStatus::isValid() const {
            // 基本验证逻辑
            return !data_.device.device_id.empty() &&
                   !data_.device.sn.empty() &&
                   !event_type_.empty();
        }

        std::string DeviceStatus::toString() const {
            std::stringstream ss;
            ss << "DeviceStatus:\n";
            ss << "  Event Type: " << event_type_ << "\n";
            ss << "  Action Type: " << action_type_ << "\n";
            ss << "  Device ID: " << data_.device.device_id << "\n";
            ss << "  Printer Name: " << data_.device.printer_name << "\n";
            ss << "  Status: " << data_.device.status << "\n";
            ss << "  Progress: " << data_.progress.progress << "%\n";
            ss << "  Filament Left: " << data_.materials.left_filament << "\n";
            ss << "  Filament Right: " << data_.materials.right_filament << "\n";
            return ss.str();
        }

        std::string DeviceStatus::toJsonString() const {
            std::stringstream ss;
            ss << "{\n";
            ss << "  \"event_type\": \"" << event_type_ << "\",\n";
            ss << "  \"action_type\": \"" << action_type_ << "\",\n";
            ss << "  \"device_id\": \"" << data_.device.device_id << "\",\n";
            ss << "  \"printer_name\": \"" << data_.device.printer_name << "\",\n";
            ss << "  \"status\": \"" << data_.device.status << "\",\n";
            ss << "  \"progress\": " << data_.progress.progress << ",\n";
            ss << "  \"current_temperature\": {\n";
            ss << "    \"chamber\": " << data_.temperatures.chamber_temp << ",\n";
            ss << "    \"left_nozzle\": " << data_.temperatures.left_temperature << ",\n";
            ss << "    \"right_nozzle\": " << data_.temperatures.right_temperature << "\n";
            ss << "  },\n";
            ss << "  \"filament\": {\n";
            ss << "    \"left\": " << data_.materials.left_filament << ",\n";
            ss << "    \"right\": " << data_.materials.right_filament << "\n";
            ss << "  }\n";
            ss << "}";
            return ss.str();
        }

    } // namespace models
} // namespace swan