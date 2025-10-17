// ==============================================================================
// @file   monitoring.cpp
// @brief  性能监控工具类的实现
// ==============================================================================

#include "monitoring.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

/**
 * @brief 获取当前CPU使用率。
 * @return double CPU使用率百分比。
 */
double Monitoring::get_cpu_usage() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0;
    }

    std::string line;
    std::getline(file, line);
    
    // 解析CPU统计信息
    std::istringstream iss(line);
    std::string cpu_label;
    unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
    
    iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal >> guest >> guest_nice;
    
    unsigned long idle_time = idle + iowait;
    unsigned long total_time = user + nice + system + idle + iowait + irq + softirq + steal;
    
    static unsigned long prev_idle = 0, prev_total = 0;
    
    unsigned long delta_idle = idle_time - prev_idle;
    unsigned long delta_total = total_time - prev_total;
    
    double cpu_usage = 0.0;
    if (delta_total != 0) {
        cpu_usage = 100.0 * (delta_total - delta_idle) / delta_total;
    }
    
    prev_idle = idle_time;
    prev_total = total_time;
    
    return cpu_usage;
}

/**
 * @brief 获取当前内存使用情况。
 * @return std::string 包含内存使用信息的字符串。
 */
std::string Monitoring::get_memory_info() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return "Memory(MB): unavailable";
    }

    double total_kb = 0.0;
    double free_kb = 0.0;
    double available_kb = 0.0;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        double value = 0.0;
        std::string unit;
        iss >> key >> value >> unit;

        if (key == "MemTotal:") {
            total_kb = value;
        } else if (key == "MemFree:") {
            free_kb = value;
        } else if (key == "MemAvailable:") {
            available_kb = value;
        }

        if (total_kb > 0.0 && free_kb > 0.0 && available_kb > 0.0) {
            break;
        }
    }

    if (total_kb <= 0.0) {
        return "Memory(MB): unavailable";
    }

    const double total_mb = total_kb / 1024.0;
    const double free_mb = free_kb / 1024.0;
    const double available_mb = available_kb / 1024.0;
    const double used_mb = std::max(0.0, total_mb - available_mb);

    std::ostringstream result;
    result << std::fixed << std::setprecision(3)
           << "Memory(MB): total=" << total_mb
           << ", used=" << used_mb
           << ", free=" << free_mb
           << ", available=" << available_mb;
    return result.str();
}

/**
 * @brief 获取当前磁盘使用情况。
 * @return std::string 包含磁盘使用信息的字符串。
 */
std::string Monitoring::get_disk_usage() {
    // 这里我们简单返回当前目录的磁盘使用情况
    // 在实际应用中，这可能需要更复杂的实现
    std::ostringstream result;
    result << "Disk usage information for the application";
    return result.str();
}

/**
 * @brief 记录开始时间，用于性能分析。
 */
void Monitoring::start_timing() {
    start_time_ = std::chrono::steady_clock::now();
}

/**
 * @brief 记录结束时间并返回经过的时间。
 * @return double 经过的时间（毫秒）。
 */
double Monitoring::stop_timing() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
    return static_cast<double>(duration.count());
}