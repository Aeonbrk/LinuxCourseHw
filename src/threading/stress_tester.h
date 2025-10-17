// ==============================================================================
// @file   stress_tester.h
// @brief  长时间压力测试工具类的声明
// ==============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../core/filesystem.h"

// ==============================================================================
// 压力测试配置
// ==============================================================================

/**
 * @struct StressTestConfig
 * @brief 压力测试所需的配置参数集合。
 */
struct StressTestConfig {
  std::chrono::seconds duration{std::chrono::hours(12)};  ///< 运行时长
  std::size_t file_count{50};                             ///< 文件数量
  std::size_t thread_count{8};                            ///< 工作线程数
  std::size_t write_size{4096};                           ///< 单次写入大小（字节）
  std::chrono::milliseconds monitor_interval{
      std::chrono::seconds(30)};                          ///< 监控输出间隔
  std::string workspace_path{"/stress_suite"};          ///< 工作目录
  bool cleanup_after{false};                              ///< 是否在完成后清理
  std::size_t bucket_count{0};                            ///< 子目录数量（0表示自动）
};

// ==============================================================================
// 压力测试执行器
// ==============================================================================

/**
 * @class StressTester
 * @brief 负责在指定文件系统上执行压力测试。
 */
class StressTester {
 public:
  /**
   * @brief 构造函数。
   * @param fs 文件系统引用。
   */
  explicit StressTester(FileSystem& fs);

  /**
   * @brief 运行压力测试。
   * @param config 压力测试配置参数。
   * @return bool 成功返回true，失败返回false。
   */
  bool run(const StressTestConfig& config);

 private:
  /**
   * @brief 确保工作目录和文件已准备就绪。
   * @param config 压力测试配置。
   * @return bool 准备成功返回true。
   */
  bool prepare_workspace(const StressTestConfig& config);

  /**
   * @brief 清理工作目录。
   * @param config 压力测试配置。
   */
  void cleanup_workspace(const StressTestConfig& config);

  /**
   * @brief 构建指定索引的文件路径。
   * @param config 压力测试配置。
   * @param index 文件索引。
   * @return std::string 文件路径。
   */
  [[nodiscard]] std::string build_file_path(const StressTestConfig& config,
                                            std::size_t index) const;
  [[nodiscard]] std::string build_bucket_path(const StressTestConfig& config,
                                              std::size_t index) const;

  /**
   * @brief 工作者线程执行循环。
   * @param worker_id 工作者编号。
   * @param config 压力测试配置。
   * @param stop_flag 停止标志。
   * @param operation_counter 操作计数器。
   * @param error_counter 错误计数器。
   */
  void worker_loop(std::size_t worker_id, const StressTestConfig& config,
                   std::atomic<bool>& stop_flag,
                   std::atomic<std::uint64_t>& operation_counter,
                   std::atomic<std::uint64_t>& error_counter) const;

  /**
   * @brief 监控线程循环。
   * @param config 压力测试配置。
   * @param stop_flag 停止标志。
   * @param operation_counter 操作计数器。
   * @param error_counter 错误计数器。
   */
  void monitor_loop(const StressTestConfig& config,
                    std::atomic<bool>& stop_flag,
                    std::atomic<std::uint64_t>& operation_counter,
                    std::atomic<std::uint64_t>& error_counter,
                    std::chrono::steady_clock::time_point start_time) const;

  [[nodiscard]] bool ensure_file_available(const std::string& path) const;
  void cleanup_directory_recursive(const std::string& path) const;

  FileSystem& filesystem_;  ///< 文件系统引用
};

// ==============================================================================
// 辅助接口
// ==============================================================================

/**
 * @brief 运行具有默认配置的压力测试。
 * @param fs 文件系统引用。
 * @return int 成功返回0，失败返回1。
 */
int run_stress_test(FileSystem& fs);

/**
 * @brief 运行自定义配置的压力测试。
 * @param fs 文件系统引用。
 * @param config 压力测试配置。
 * @return int 成功返回0，失败返回1。
 */
int run_stress_test(FileSystem& fs, const StressTestConfig& config);

/**
 * @brief 解析命令行参数生成压力测试配置。
 * @param args 参数列表（不包含关键字"stress"）。
 * @param[out] config 输出配置。
 * @param[out] error_message 错误信息。
 * @return bool 成功返回true。
 */
bool parse_stress_arguments(const std::vector<std::string>& args,
                            StressTestConfig& config,
                            std::string& error_message);
