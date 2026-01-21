//
// Created by wave on 2026/1/21.
//

#pragma once
#include <memory>
#include <mutex>

namespace swan {
    namespace utils {

        template<typename T>
        class Singleton {
        public:
            // 删除拷贝构造函数和赋值运算符
            Singleton(const Singleton&) = delete;
            Singleton& operator=(const Singleton&) = delete;

            // 获取单例实例
            static T& getInstance() {
                static std::once_flag initFlag;
                std::call_once(initFlag, []() {
                    instance_ = std::make_unique<T>();
                });
                return *instance_;
            }

            // 获取指针
            static T* getInstancePtr() {
                return &getInstance();
            }

            // 销毁实例（主要用于测试或特殊场景）
            static void destroyInstance() {
                instance_.reset();
            }

        protected:
            Singleton() = default;
            virtual ~Singleton() = default;

        private:
            static std::unique_ptr<T> instance_;
        };

        // 静态成员初始化
        template<typename T>
        std::unique_ptr<T> Singleton<T>::instance_ = nullptr;

    } // namespace utils
} // namespace swan