#pragma once

#include <string>
#include <chrono>

/**
 * @class Monitoring
 * @brief 性能监控工具类。
 * 
 * 提供CPU、内存和磁盘使用情况的监控功能。
 */
class Monitoring {
public:
    /**
     * @brief 构造函数。
     */
    Monitoring() = default;

    /**
     * @brief 析构函数。
     */
    ~Monitoring() = default;

    /**
     * @brief 获取当前CPU使用率。
     * @return double CPU使用率百分比。
     */
    static double get_cpu_usage();

    /**
     * @brief 获取当前内存使用情况。
     * @return std::string 包含内存使用信息的字符串。
     */
    static std::string get_memory_info();

    /**
     * @brief 获取当前磁盘使用情况。
     * @return std::string 包含磁盘使用信息的字符串。
     */
    static std::string get_disk_usage();

    /**
     * @brief 记录开始时间，用于性能分析。
     */
    void start_timing();

    /**
     * @brief 记录结束时间并返回经过的时间。
     * @return double 经过的时间（毫秒）。
     */
    double stop_timing();

private:
    std::chrono::steady_clock::time_point start_time_;  ///< 开始时间
};