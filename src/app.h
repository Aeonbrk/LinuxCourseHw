#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cli/cli_interface.h"
#include "core/filesystem.h"
#include "utils/error_handler.h"
#include "utils/path_utils.h"

// Forward declaration for threading components
class TaskDispatcher;

/**
 * @class App
 * @brief 应用程序的主类。
 *
 * 负责解析命令行参数，并根据参数初始化文件系统、执行相应操作，
 * 如创建磁盘、格式化磁盘或启动交互式CLI。
 */
class App {
 public:
  /**
   * @brief 构造函数。
   * @param argc 命令行参数的数量。
   * @param argv 命令行参数的数组。
   */
  App(int argc, char* argv[]);

  /**
   * @brief 运行应用程序，为程序的总入口。
   * @return int 应用程序的退出码。
   */
  int run();

  /**
   * @brief 析构函数。
   */
  ~App();

 private:
  /**
   * @brief 打印程序的使用方法和命令说明。
   */
  void print_usage();

  /**
   * @brief 处理 'create' 命令，用于创建新的虚拟磁盘文件。
   * @return int 成功返回0，失败返回1。
   */
  int handle_create_command();

  /**
   * @brief 处理 'format' 命令，用于格式化虚拟磁盘。
   * @return int 成功返回0，失败返回1。
   */
  int handle_format_command();

  /**
   * @brief 处理 'run' 命令（交互模式）或单个文件系统命令。
   * @return int 成功返回0，失败返回1。
   */
  int handle_run_command();

  /**
   * @brief 处理多线程任务分发模式。
   * @return int 成功返回0，失败返回1。
   */
  int handle_multithreaded_mode();

  /**
   * @brief 处理压力测试命令。
   * @return int 成功返回0，失败返回1。
   */
  int handle_stress_command();

  int _argc;                       // 命令行参数数量
  std::vector<std::string> _argv;  // 命令行参数列表
  std::string _program_name;       // 程序名称
  std::string _disk_path;          // 磁盘文件路径
  FileSystem _fs;                  // 文件系统实例
  std::unique_ptr<TaskDispatcher>
      _task_dispatcher;  // 任务分发器实例（在多线程模式下使用）
};