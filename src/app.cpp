// ==============================================================================
// @file   app.cpp
// @brief  应用程序主类的实现
// ==============================================================================

#include "app.h"
#include <iostream>
#include "utils/common.h"
#include "utils/error_codes.h"
#include "utils/path_utils_extended.h"
#include "utils/app_utils.h"
#include "threading/stress_tester.h"
#include "threading/task_dispatcher.h"

/**
 * @brief App 类的构造函数。
 * @param argc 命令行参数的数量。
 * @param argv 命令行参数的数组。
 */
App::App(int argc, char* argv[]) : _argc(argc) {
  for (int i = 0; i < argc; ++i) {
    _argv.push_back(argv[i]);
  }
  if (argc > 0) {
    _program_name = _argv[0];
  }
}

/**
 * @brief 运行应用程序，解析并分发命令。
 * @return int 应用程序的退出码（0表示成功）。
 */
int App::run() {
  // 至少需要程序名和磁盘路径两个参数
  if (!AppUtils::validate_args(_argc, nullptr, 2)) {
    print_usage();
    return 1;
  }

  _disk_path = PathUtils::normalize_path(_argv[1]);

  // 根据命令分发到不同的处理函数
  if (_argc >= 4 && _argv[2] == "create") {
    return handle_create_command();
  }

  if (_argc >= 3 && _argv[2] == "format") {
    return handle_format_command();
  }
  
  // 检查是否是多线程模式
  if (_argc >= 3 && _argv[2] == "stress") {
    return handle_stress_command();
  }

  if (_argc >= 3 && _argv[2] == "multithreaded") {
    return handle_multithreaded_mode();
  }

  return handle_run_command();
}

/**
 * @brief 打印程序的使用说明。
 */
void App::print_usage() {
  std::cout << "Usage: " << _program_name << " <disk_file> [command]" << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;
  std::cout << "  create <size_mb>     - Create a new disk file" << std::endl;
  std::cout << "  format               - Format the disk" << std::endl;
  std::cout << "  run                  - Run interactive shell" << std::endl;
  std::cout << "  stress [options]     - Run storage stress test" << std::endl;
  std::cout << "  multithreaded <cmd>  - Execute a command using multithreaded dispatcher" << std::endl;
  std::cout << "  <command>            - Execute a single command" << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << _program_name << " disk.img create 100" << std::endl;
  std::cout << "  " << _program_name << " disk.img format" << std::endl;
  std::cout << "  " << _program_name << " disk.img run" << std::endl;
  std::cout << "  " << _program_name << " disk.img ls /" << std::endl;
  std::cout << "  " << _program_name << " disk.img multithreaded ls /" << std::endl;
}

/**
 * @brief 处理 'create' 命令，创建虚拟磁盘文件。
 * @return int 成功返回0，失败返回1。
 */
int App::handle_create_command() {
  int size_mb;
  if (!AppUtils::try_stoi(_argv[3], size_mb)) {
    return 1;
  }

  DiskSimulator disk;
  if (!ErrorHandler::check_and_log(
      disk.create_disk(_disk_path, size_mb),
      ERROR_IO_ERROR,
      "Failed to create disk: " + _disk_path
  )) {
    return 1;
  }

  std::cout << "Disk created successfully: " << _disk_path << " (" << size_mb << "MB)" << std::endl;
  return 0;
}

/**
 * @brief 处理 'format' 命令，格式化磁盘。
 * @return int 成功返回0，失败返回1。
 */
int App::handle_format_command() {
  DiskSimulator disk;
  if (!ErrorHandler::check_and_log(
      disk.open_disk(_disk_path),
      ERROR_IO_ERROR,
      "Cannot open disk file for formatting: " + _disk_path
  )) {
    return 1;
  }

  if (!ErrorHandler::check_and_log(
      disk.format_disk(),
      ERROR_FORMAT_FAILED,
      "Failed to format disk: " + _disk_path
  )) {
    disk.close_disk();
    return 1;
  }

  std::cout << "Disk formatted successfully" << std::endl;
  disk.close_disk();
  return 0;
}

/**
 * @brief 处理 'run' 命令或单个文件系统命令。
 * @return int 成功返回0，失败返回1。
 */
int App::handle_run_command() {
  if (!ErrorHandler::check_and_log(
      _fs.mount(_disk_path),
      ERROR_NOT_MOUNTED,
      "Cannot mount disk file: " + _disk_path + " (Make sure the disk file exists and is formatted.)"
  )) {
    return 1;
  }

  CLIInterface cli(_fs);
  // 进入交互模式
  if (_argc < 3 || _argv[2] == "run") {
    cli.run();
  } else { // 执行单个命令
    std::string cmd_line;
    for (size_t i = 2; i < _argv.size(); ++i) {
      if (i > 2) cmd_line += " ";
      cmd_line += _argv[i];
    }
    
    Command cmd;
    if (cli.get_parser().parse_line(cmd_line, cmd)) {
      if (!cli.execute_command(cmd)) {
        _fs.unmount();
        return 1; // 命令执行失败
      }
    } else {
      _fs.unmount();
      return 1; // 命令解析失败
    }
  }

  _fs.unmount();
  return 0;
}

int App::handle_multithreaded_mode() {
  if (!ErrorHandler::check_and_log(
      _fs.mount(_disk_path),
      ERROR_NOT_MOUNTED,
      "Cannot mount disk file: " + _disk_path + " (Make sure the disk file exists and is formatted.)"
  )) {
    return 1;
  }

  size_t thread_count = 4;
  size_t command_start_index = 3;

  if (_argc > 5 && _argv[3] == "--threads") {
    int parsed_threads = 0;
    if (!AppUtils::try_stoi(_argv[4], parsed_threads) || parsed_threads <= 0) {
      std::cout << "Invalid thread count specified for multithreaded mode" << std::endl;
      _fs.unmount();
      return 1;
    }
    thread_count = static_cast<size_t>(parsed_threads);
    command_start_index = 5;
  }

  if (_argc <= static_cast<int>(command_start_index)) {
    std::cout << "Multithreaded mode requires at least one command" << std::endl;
    std::cout << "Example: " << _program_name << " disk.img multithreaded touch /a.txt" << std::endl;
    _fs.unmount();
    return 1;
  }

  _task_dispatcher = std::make_unique<TaskDispatcher>(_fs, thread_count);

  std::string raw_commands;
  for (size_t i = command_start_index; i < _argv.size(); ++i) {
    if (i > command_start_index) raw_commands += " ";
    raw_commands += _argv[i];
  }

  auto split_commands = [](const std::string& input) {
    std::vector<std::string> commands;
    std::string current;
    for (char ch : input) {
      if (ch == ';') {
        if (!current.empty()) {
          commands.push_back(current);
          current.clear();
        }
      } else {
        current += ch;
      }
    }
    if (!current.empty()) {
      commands.push_back(current);
    }
    return commands;
  };

  auto trim = [](std::string& text) {
    auto start = text.find_first_not_of(" \t\r\n");
    auto end = text.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) {
      text.clear();
    } else {
      text = text.substr(start, end - start + 1);
    }
  };

  std::vector<std::string> commands = split_commands(raw_commands);
  if (commands.empty()) {
    trim(raw_commands);
    if (!raw_commands.empty()) {
      commands.push_back(raw_commands);
    }
  }

  std::vector<std::future<int>> futures;
  futures.reserve(commands.size());

  for (auto& command : commands) {
    trim(command);
    if (!command.empty()) {
      futures.emplace_back(_task_dispatcher->execute_async(command));
    }
  }

  int result_code = 0;
  for (auto& future : futures) {
    if (future.get() != 0) {
      result_code = 1;
    }
  }

  _task_dispatcher.reset();
  _fs.unmount();
  return result_code;
}

int App::handle_stress_command() {
  std::vector<std::string> args;
  for (std::size_t index = 3; index < _argv.size(); ++index) {
    args.push_back(_argv[index]);
  }

  StressTestConfig config;
  std::string error_message;
  if (!parse_stress_arguments(args, config, error_message)) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT, error_message);
    return 1;
  }

  if (!ErrorHandler::check_and_log(
          _fs.mount(_disk_path), ERROR_NOT_MOUNTED,
          "Cannot mount disk file: " + _disk_path +
              " (Make sure the disk file exists and is formatted.)")) {
    return 1;
  }

  int result = run_stress_test(_fs, config);
  _fs.unmount();

  if (result == 0) {
    std::cout << "[Stress] Test finished successfully" << std::endl;
  } else {
    std::cout << "[Stress] Test finished with errors" << std::endl;
  }

  return result;
}

// App Destructor
App::~App() = default;
