
//
// Created by wave on 2026/1/23.
//
#pragma once

#include <string>
#include <optional>
#include <vector>
#include <chrono>
#include <any>
#include "utils/error.h"

namespace swan {
namespace common {

class Result {
public:
    // 构造成功结果
    static Result success(const std::string& message = "") {
        return Result(SUCCESS, message);
    }

    // 构造失败结果
    static Result failure(ErrorCode code, const std::string& message = "",
                         const std::string& detail = "") {
        return Result(code, message, detail);
    }

    // 默认构造函数（成功）
    Result() : error_info_(SUCCESS, "") {}

    // 带错误码的构造函数
    Result(ErrorCode code, const std::string& message = "",
           const std::string& detail = "")
        : error_info_(code, message, detail) {}

    // 设置成功
    void setSuccess(const std::string& message = "") {
        error_info_ = ErrorInfo(SUCCESS, message);
    }

    // 设置错误
    void setError(ErrorCode code, const std::string& message = "",
                  const std::string& detail = "") {
        error_info_ = ErrorInfo(code, message, detail);
    }

    // 获取错误信息
    const ErrorInfo& getErrorInfo() const { return error_info_; }

    // 检查是否成功
    bool isSuccess() const { return error_info_.isSuccess(); }

    // 获取错误码
    ErrorCode getErrorCode() const { return error_info_.getCode(); }

    // 获取错误消息
    std::string getErrorMessage() const { return error_info_.getMessage(); }

    // 获取详细错误信息
    std::string getErrorDetail() const { return error_info_.getDetail(); }

    // 转换为字符串
    std::string toString() const {
        return error_info_.toString();
    }

    // 携带额外数据
    template<typename T>
    Result& setData(const T& data) {
        data_ = data;
        return *this;
    }

    template<typename T>
    std::optional<T> getData() const {
        try {
            return std::any_cast<T>(data_);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    // 检查是否有数据
    template<typename T>
    bool hasData() const {
        return data_.type() == typeid(T);
    }

    // 执行时间统计
    void setExecutionTime(std::chrono::microseconds time) {
        execution_time_ = time;
    }

    void setExecutionTime(uint64_t microseconds) {
        execution_time_ = std::chrono::microseconds(microseconds);
    }

    std::chrono::microseconds getExecutionTime() const { return execution_time_; }

    // 添加子结果
    void addSubResult(const Result& sub_result) {
        sub_results_.push_back(sub_result);
    }

    const std::vector<Result>& getSubResults() const {
        return sub_results_;
    }

    // 合并多个结果
    static Result merge(const std::vector<Result>& results) {
        if (results.empty()) {
            return Result::success();
        }

        for (const auto& result : results) {
            if (!result.isSuccess()) {
                return result;
            }
        }

        Result merged = Result::success();
        for (const auto& result : results) {
            merged.addSubResult(result);
        }
        return merged;
    }

private:
    ErrorInfo error_info_;
    std::chrono::microseconds execution_time_{0};
    std::any data_;  // 使用 std::any 存储任意类型数据
    std::vector<Result> sub_results_;
};

} // namespace common
} // namespace swan