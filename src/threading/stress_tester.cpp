// ==============================================================================
// @file   stress_tester.cpp
// @brief  长时间压力测试工具类的实现
// ==============================================================================

#include "stress_tester.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <unordered_set>

#include "../utils/common.h"
#include "../utils/error_handler.h"
#include "../utils/monitoring.h"
#include "../utils/path_utils.h"

namespace {

/**
 * @brief 将字符串解析为正整数。
 * @param value_str 输入字符串。
 * @param[out] value 输出数值。
 * @return bool 成功返回true。
 */
bool parse_positive_int(const std::string& value_str, std::uint64_t& value) {
  try {
    if (value_str.empty()) {
      return false;
    }
    std::size_t processed = 0;
    long long parsed = std::stoll(value_str, &processed);
    if (processed != value_str.size() || parsed <= 0) {
      return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace

/**
 * @brief StressTester 构造函数。
 * @param fs 文件系统引用。
 */
StressTester::StressTester(FileSystem& fs) : filesystem_(fs) {}

/**
 * @brief 运行压力测试主流程。
 * @param config 压力测试配置。
 * @return bool 成功返回true。
 */
bool StressTester::run(const StressTestConfig& config) {
  StressTestConfig normalized_config = config;
  if (normalized_config.workspace_path.empty()) {
    normalized_config.workspace_path = "/stress_suite";
  }
  if (normalized_config.workspace_path.front() != '/') {
    normalized_config.workspace_path = "/" + normalized_config.workspace_path;
  }
  normalized_config.workspace_path =
      PathUtils::normalize_path(normalized_config.workspace_path);
  if (normalized_config.bucket_count == 0) {
    normalized_config.bucket_count =
        std::max<std::size_t>(normalized_config.thread_count, 1);
  }
  normalized_config.bucket_count =
      std::max<std::size_t>(normalized_config.bucket_count, 1);
  normalized_config.bucket_count =
      std::min<std::size_t>(normalized_config.bucket_count,
                            std::max<std::size_t>(normalized_config.file_count,
                                                   static_cast<std::size_t>(1)));

  if (!filesystem_.is_mounted()) {
    ErrorHandler::log_error(ERROR_NOT_MOUNTED,
                            "File system must be mounted for stress test");
    return false;
  }

  if (normalized_config.file_count == 0 ||
      normalized_config.thread_count == 0 ||
      normalized_config.write_size == 0) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                            "Invalid stress test configuration");
    return false;
  }

  if (!prepare_workspace(normalized_config)) {
    return false;
  }

  std::cout << "[Stress] Starting stress test with "
            << normalized_config.file_count << " files, "
            << normalized_config.thread_count
            << " threads, duration " << normalized_config.duration.count()
            << " seconds" << std::endl;

  std::atomic<bool> stop_flag{false};
  std::atomic<std::uint64_t> operation_counter{0};
  std::atomic<std::uint64_t> error_counter{0};

  std::vector<std::thread> workers;
  workers.reserve(normalized_config.thread_count);

  for (std::size_t worker_id = 0; worker_id < normalized_config.thread_count;
       ++worker_id) {
    workers.emplace_back([this, worker_id, &normalized_config, &stop_flag,
                          &operation_counter, &error_counter]() {
      worker_loop(worker_id, normalized_config, stop_flag, operation_counter,
                  error_counter);
    });
  }

  const auto test_start = std::chrono::steady_clock::now();

  std::thread monitor_thread([this, &normalized_config, &stop_flag,
                              &operation_counter, &error_counter,
                              test_start]() {
    monitor_loop(normalized_config, stop_flag, operation_counter,
                 error_counter, test_start);
  });

  while (std::chrono::steady_clock::now() - test_start <
         normalized_config.duration) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  stop_flag.store(true);

  for (auto& worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }

  if (monitor_thread.joinable()) {
    monitor_thread.join();
  }

  const auto now = std::chrono::steady_clock::now();
  const double elapsed_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(now - test_start)
          .count();
  const std::uint64_t total_operations =
      operation_counter.load(std::memory_order_relaxed);
  const std::uint64_t total_errors =
      error_counter.load(std::memory_order_relaxed);
  const double avg_ops_rate =
      elapsed_seconds > 0.0
          ? static_cast<double>(total_operations) / elapsed_seconds
          : 0.0;

  std::ostringstream final_metrics;
  final_metrics << std::fixed << std::setprecision(3)
                << "[Stress] Completed | elapsed_s: " << elapsed_seconds
                << " | ops_total: " << total_operations
                << " | avg_ops_rate: "
                << std::setprecision(3) << avg_ops_rate << " ops/s"
                << " | errors_total: " << total_errors;
  std::cout << final_metrics.str() << std::endl;

  if (normalized_config.cleanup_after) {
    cleanup_workspace(normalized_config);
  }

  return error_counter.load() == 0;
}

/**
 * @brief 准备压力测试所需的工作目录与文件。
 * @param config 压力测试配置。
 * @return bool 准备成功返回true。
 */
bool StressTester::prepare_workspace(const StressTestConfig& config) {
  if (!filesystem_.file_exists(config.workspace_path)) {
    if (!filesystem_.create_directory(config.workspace_path)) {
      return false;
    }
  }

  std::unordered_set<std::string> prepared_directories;
  prepared_directories.insert(config.workspace_path);

  for (std::size_t index = 0; index < config.file_count; ++index) {
    const std::string bucket_path = build_bucket_path(config, index);
    if (prepared_directories.insert(bucket_path).second) {
      if (!filesystem_.file_exists(bucket_path)) {
        if (!filesystem_.create_directory(bucket_path)) {
          return false;
        }
      }
    }

    const std::string path = build_file_path(config, index);
    if (filesystem_.file_exists(path)) {
      continue;
    }
    const int inode = filesystem_.create_file(
        path, FILE_PERMISSION_READ | FILE_PERMISSION_WRITE);
    if (inode == -1) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 清理工作目录及其文件。
 * @param config 压力测试配置。
 */
void StressTester::cleanup_workspace(const StressTestConfig& config) {
  cleanup_directory_recursive(config.workspace_path);
  filesystem_.remove_directory(config.workspace_path);
}

/**
 * @brief 构建文件路径。
 * @param config 压力测试配置。
 * @param index 文件索引。
 * @return std::string 目标文件路径。
 */
std::string StressTester::build_bucket_path(const StressTestConfig& config,
                                            std::size_t index) const {
  if (config.bucket_count <= 1) {
    return config.workspace_path;
  }

  std::ostringstream oss;
  oss << config.workspace_path;
  if (config.workspace_path.back() != '/') {
    oss << '/';
  }

  const std::size_t bucket = index % config.bucket_count;
  oss << "bucket_" << std::setw(3) << std::setfill('0') << bucket;
  return oss.str();
}

std::string StressTester::build_file_path(const StressTestConfig& config,
                                          std::size_t index) const {
  const std::string bucket_path = build_bucket_path(config, index);

  std::ostringstream oss;
  oss << bucket_path;
  if (bucket_path.back() != '/') {
    oss << '/';
  }

  oss << "file_" << std::setw(3) << std::setfill('0') << index << ".dat";
  return oss.str();
}

/**
 * @brief 工作者线程循环。
 * @param worker_id 工作者编号。
 * @param config 压力测试配置。
 * @param stop_flag 停止标志。
 * @param operation_counter 操作计数器。
 * @param error_counter 错误计数器。
 */
void StressTester::worker_loop(
    std::size_t worker_id, const StressTestConfig& config,
    std::atomic<bool>& stop_flag, std::atomic<std::uint64_t>& operation_counter,
    std::atomic<std::uint64_t>& error_counter) const {
  std::vector<char> write_buffer(config.write_size, 0);
  std::vector<char> read_buffer(config.write_size, 0);

  std::size_t iteration = 0;

  while (!stop_flag.load(std::memory_order_relaxed)) {
    for (std::size_t index = worker_id;
         index < config.file_count &&
         !stop_flag.load(std::memory_order_relaxed);
         index += config.thread_count) {
      const std::string path = build_file_path(config, index);

      if (!ensure_file_available(path)) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      const char fill_char = static_cast<char>('A' +
                                               (worker_id + iteration) % 26);
      std::fill(write_buffer.begin(), write_buffer.end(), fill_char);

      int write_fd = filesystem_.open_file(path, OPEN_MODE_WRITE);
      if (write_fd == -1) {
        if (!ensure_file_available(path)) {
          error_counter.fetch_add(1, std::memory_order_relaxed);
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          continue;
        }
        write_fd = filesystem_.open_file(path, OPEN_MODE_WRITE);
      }

      if (write_fd == -1) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      if (!filesystem_.seek_file(write_fd, 0)) {
        filesystem_.close_file(write_fd);
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      const int bytes_to_write = static_cast<int>(
          std::min<std::size_t>(write_buffer.size(),
                                static_cast<std::size_t>(
                                    std::numeric_limits<int>::max())));
      const int written = filesystem_.write_file(write_fd,
                                                 write_buffer.data(),
                                                 bytes_to_write);
      filesystem_.close_file(write_fd);

      if (written != bytes_to_write) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      operation_counter.fetch_add(1, std::memory_order_relaxed);

      const int read_fd = filesystem_.open_file(path, OPEN_MODE_READ);
      if (read_fd == -1) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      const int bytes_read =
          filesystem_.read_file(read_fd, read_buffer.data(), written);
      filesystem_.close_file(read_fd);

      if (bytes_read != written) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      if (!std::equal(write_buffer.begin(),
                      write_buffer.begin() + bytes_read,
                      read_buffer.begin())) {
        error_counter.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }

      operation_counter.fetch_add(1, std::memory_order_relaxed);
    }

    ++iteration;
  }
}

bool StressTester::ensure_file_available(const std::string& path) const {
  const std::string parent = filesystem_.get_parent_path(path);
  if (!parent.empty() && parent != path) {
    if (!filesystem_.file_exists(parent)) {
      filesystem_.create_directory(parent);
    }
  }

  if (filesystem_.file_exists(path)) {
    return true;
  }

  int inode = filesystem_.create_file(
      path, FILE_PERMISSION_READ | FILE_PERMISSION_WRITE);
  if (inode != -1) {
    return true;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  return filesystem_.file_exists(path);
}

void StressTester::cleanup_directory_recursive(const std::string& path) const {
  std::vector<DirectoryEntry> entries;
  if (!filesystem_.list_directory(path, entries)) {
    return;
  }

  for (const DirectoryEntry& entry : entries) {
    const std::string name(entry.name, entry.name_length);
    if (name == "." || name == "..") {
      continue;
    }

    const std::string child_path =
        path == "/" ? std::string{"/"} + name : path + "/" + name;

    if (filesystem_.is_directory(child_path)) {
      cleanup_directory_recursive(child_path);
      filesystem_.remove_directory(child_path);
      continue;
    }

    filesystem_.delete_file(child_path);
  }
}

/**
 * @brief 监控线程循环，定期输出资源使用信息。
 * @param config 压力测试配置。
 * @param stop_flag 停止标志。
 * @param operation_counter 操作计数器。
 * @param error_counter 错误计数器。
 */
void StressTester::monitor_loop(
    const StressTestConfig& config, std::atomic<bool>& stop_flag,
    std::atomic<std::uint64_t>& operation_counter,
    std::atomic<std::uint64_t>& error_counter,
    std::chrono::steady_clock::time_point start_time) const {
  Monitoring::get_cpu_usage();  // 初始化基线

  auto last_time = start_time;
  std::uint64_t last_operations = 0;
  std::uint64_t last_errors = 0;

  const auto format_double = [](double value, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
  };

  while (!stop_flag.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(config.monitor_interval);

    const bool should_stop = stop_flag.load(std::memory_order_relaxed);
    const auto now = std::chrono::steady_clock::now();

    const std::uint64_t operations =
        operation_counter.load(std::memory_order_relaxed);
    const std::uint64_t errors =
        error_counter.load(std::memory_order_relaxed);

    const std::uint64_t ops_delta = operations - last_operations;
    const std::uint64_t errors_delta = errors - last_errors;

    const double elapsed_total =
        std::chrono::duration_cast<std::chrono::duration<double>>(now - start_time)
            .count();
    const double elapsed_window =
        std::chrono::duration_cast<std::chrono::duration<double>>(now - last_time)
            .count();

    const double instant_rate =
        elapsed_window > 0.0
            ? static_cast<double>(ops_delta) / elapsed_window
            : 0.0;
    const double average_rate =
        elapsed_total > 0.0
            ? static_cast<double>(operations) / elapsed_total
            : 0.0;

    const double cpu_usage = Monitoring::get_cpu_usage();
    const std::string memory_info = Monitoring::get_memory_info();

    std::ostringstream metrics;
    metrics << "[Stress] Metrics | elapsed_s: "
            << format_double(elapsed_total, 3)
            << " | ops_total: " << operations
            << " | ops_delta: " << ops_delta
            << " | inst_ops_rate: " << format_double(instant_rate, 3)
            << " ops/s | avg_ops_rate: "
            << format_double(average_rate, 3) << " ops/s"
            << " | errors_total: " << errors
            << " | errors_delta: " << errors_delta
            << " | cfg_threads: " << config.thread_count
            << " | cfg_files: " << config.file_count
            << " | write_size_bytes: " << config.write_size
            << " | cpu: " << format_double(cpu_usage, 2) << "% | "
            << memory_info;

    std::cout << metrics.str() << std::endl;

    last_time = now;
    last_operations = operations;
    last_errors = errors;

    if (should_stop) {
      break;
    }
  }
}

/**
 * @brief 使用默认配置运行压力测试。
 * @param fs 文件系统引用。
 * @return int 成功返回0。
 */
int run_stress_test(FileSystem& fs) {
  StressTester tester(fs);
  StressTestConfig config;
  return tester.run(config) ? 0 : 1;
}

/**
 * @brief 使用指定配置运行压力测试。
 * @param fs 文件系统引用。
 * @param config 压力测试配置。
 * @return int 成功返回0。
 */
int run_stress_test(FileSystem& fs, const StressTestConfig& config) {
  StressTester tester(fs);
  return tester.run(config) ? 0 : 1;
}

/**
 * @brief 解析压力测试命令参数。
 * @param args 参数列表。
 * @param[out] config 输出配置。
 * @param[out] error_message 错误信息。
 * @return bool 成功返回true。
 */
bool parse_stress_arguments(const std::vector<std::string>& args,
                            StressTestConfig& config,
                            std::string& error_message) {
  config = StressTestConfig{};

  for (std::size_t index = 0; index < args.size(); ++index) {
    const std::string& arg = args[index];

    if (arg == "--cleanup") {
      config.cleanup_after = true;
      continue;
    }

    auto require_value = [&](const std::string& option,
                             std::uint64_t& output_value) -> bool {
      if (index + 1 >= args.size()) {
        error_message = option + " requires a value";
        return false;
      }
      std::uint64_t parsed = 0;
      if (!parse_positive_int(args[index + 1], parsed)) {
        error_message = "Invalid value for " + option + ": " + args[index + 1];
        return false;
      }
      output_value = parsed;
      ++index;
      return true;
    };

    if (arg == "--duration") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.duration = std::chrono::seconds(value);
    } else if (arg == "--files") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.file_count = static_cast<std::size_t>(value);
    } else if (arg == "--threads") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.thread_count = static_cast<std::size_t>(value);
    } else if (arg == "--write-size") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.write_size = static_cast<std::size_t>(value);
    } else if (arg == "--monitor") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.monitor_interval = std::chrono::seconds(value);
    } else if (arg == "--workspace") {
      if (index + 1 >= args.size()) {
        error_message = "--workspace requires a value";
        return false;
      }
      ++index;
      config.workspace_path = args[index];
    } else if (arg == "--buckets") {
      std::uint64_t value = 0;
      if (!require_value(arg, value)) {
        return false;
      }
      config.bucket_count = static_cast<std::size_t>(value);
    } else {
      error_message = "Unknown stress option: " + arg;
      return false;
    }
  }

  if (config.workspace_path.empty()) {
    config.workspace_path = "/stress_suite";
  }
  if (config.workspace_path.front() != '/') {
    config.workspace_path = "/" + config.workspace_path;
  }

  return true;
}
