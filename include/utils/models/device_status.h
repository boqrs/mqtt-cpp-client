//
// Created by wave on 2026/1/12.
//
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "device_status.pb.h"

namespace swan {
    namespace models {

/**
 * 打印机材料槽信息
 */
        struct MaterialSlot {
            bool has_filament;
            std::string material_color;
            std::string material_name;
            uint32_t slot_id;

            MaterialSlot() : has_filament(false), slot_id(0) {}

            // 从Protobuf转换
            static MaterialSlot fromProto(const SomeMessage_Slot_infos& proto);

            // 转换为Protobuf
            void toProto(SomeMessage_Slot_infos* proto) const;
        };

/**
 * 材料站信息
 */
        struct MaterialStation {
            uint32_t current_load_slot;
            uint32_t current_slot;
            uint32_t slot_cnt;
            std::vector<MaterialSlot> slot_infos;
            uint32_t state_action;
            uint32_t state_step;

            MaterialStation() : current_load_slot(0), current_slot(0),
                                slot_cnt(0), state_action(0), state_step(0) {}

            // 从Protobuf转换
            static MaterialStation fromProto(const SomeMessage_Matl_station_info& proto);

            // 转换为Protobuf
            void toProto(SomeMessage_Matl_station_info* proto) const;

            // 获取指定槽位的材料信息
            const MaterialSlot* getSlotInfo(uint32_t slot_id) const;

            // 检查是否有可用材料
            bool hasAvailableMaterial() const;
        };

/**
 * 设备状态数据
 */
        struct DeviceStatusData {
            // 温度相关
            struct TemperatureInfo {
                int32_t chamber_temp;
                uint32_t chamber_target_temp;
                uint32_t left_temperature;
                uint32_t left_target_temperature;
                uint32_t right_temperature;
                uint32_t right_target_temperature;
                uint32_t platform_cur_temperature;
                uint32_t platform_target_temperature;
                std::vector<uint32_t> nozzle_temps;
                std::vector<uint32_t> nozzle_target_temps;
            };

            // 打印进度相关
            struct PrintProgress {
                uint32_t progress;
                uint32_t print_layer;
                uint32_t target_layer;
                uint32_t estimate_time;
                uint32_t actual_duration;
                uint32_t duration;
                uint32_t cumulative_print_time;
                std::string file_name;
                std::string file_path;
            };

            // 材料相关
            struct MaterialInfo {
                uint32_t left_filament;
                uint32_t right_filament;
                std::string left_filament_type;
                std::string right_filament_type;
                double cumulative_filament;
                MaterialStation material_station;
            };

            // 设备信息
            struct DeviceInfo {
                std::string device_id;
                std::string printer_name;
                std::string sn;
                std::string mac;
                std::string ip_address;
                std::string firmware_version;
                std::string status;
                std::string error_code;
            };

            // 成员变量
            TemperatureInfo temperatures;
            PrintProgress progress;
            MaterialInfo materials;
            DeviceInfo device;

            // 其他字段
            uint32_t camera;
            uint32_t chamber_fan;
            uint32_t cooling_fan;
            uint32_t current_speed;
            uint32_t entirety_speed;
            uint32_t estimate_length_left;
            uint32_t estimate_length_right;
            std::string delay_close;
            uint32_t delay_time;
            std::string door;
            std::string external;
            std::string internal;
            std::string job_id;
            std::string language;
            uint32_t lidar;
            std::string light;
            std::string location;
            std::string measure;  // 修正：从 uint32_t 改为 std::string
            uint32_t model_weight;
            uint32_t nozzle_count;
            std::string nozzle_model;
            uint32_t nozzle_style;
            std::string pid;
            std::string polar_register_code;
            std::string flash_register_code;
            double remain_memory;
            std::string stream;
            std::string hls_stream;
            std::string thumbnail_path;
            uint32_t tvoc;
            uint32_t z_axis_compensation;
            uint32_t filling_amount;

            DeviceStatusData();

            // 从Protobuf转换
            static DeviceStatusData fromProto(const SomeMessage_Data& proto);

            // 转换为Protobuf
            void toProto(SomeMessage_Data* proto) const;

            // 实用方法
            bool isPrinting() const;
            bool hasError() const;
            double getFilamentRemainingPercentage() const;
            std::string getFormattedRemainingTime() const;
        };

/**
 * 完整的设备状态消息
 */
        class DeviceStatus {
        public:
            DeviceStatus();
            ~DeviceStatus();

            // 设置事件类型
            void setEventType(const std::string& event_type);
            const std::string& getEventType() const;

            // 设置动作类型
            void setActionType(const std::string& action_type);
            const std::string& getActionType() const;

            // 设置设备数据
            void setData(const DeviceStatusData& data);
            const DeviceStatusData& getData() const;
            DeviceStatusData& getMutableData();

            // Protobuf转换
            static DeviceStatus fromProto(const SomeMessage& proto);
            void toProto(SomeMessage* proto) const;

            // 序列化/反序列化
            std::string serializeToString() const;
            bool parseFromString(const std::string& data);

            // 验证
            bool isValid() const;

            // 转换为可读字符串
            std::string toString() const;
            std::string toJsonString() const;

        private:
            std::string event_type_;
            std::string action_type_;
            DeviceStatusData data_;
        };
    }
}