// ==============================================================================
// @file   cli_interface.cpp
// @brief  命令行界面（CLI）的实现
// ==============================================================================

#include "cli_interface.h"

#include <cstring>
#include <iostream>
#include <vector>

#include "../threading/stress_tester.h"

/**
 * @brief CLIInterface 构造函数。
 * @param fs 文件系统对象的引用。
 */
CLIInterface::CLIInterface(FileSystem& fs) : filesystem(fs), running(false) {
}

/**
 * @brief CLIInterface 析构函数。
 */
CLIInterface::~CLIInterface() {
}

/**
 * @brief 运行CLI交互式主循环。
 */
void CLIInterface::run() {
  if (!filesystem.is_mounted()) {
    ErrorHandler::log_error(ERROR_NOT_MOUNTED, "File system not mounted");
    return;
  }

  running = true;
  std::string line;

  std::cout << "Disk Simulation System" << std::endl;
  std::cout << "Type 'help' for available commands" << std::endl;

  while (running) {
    std::cout << get_prompt();
    std::getline(std::cin, line);

    Command cmd;
    if (parser.parse_line(line, cmd)) {
      execute_command(cmd);
    }
  }
}

/**
 * @brief 执行一个已解析的命令。
 * @param cmd 要执行的命令对象。
 * @return bool 命令是否成功执行。
 */
bool CLIInterface::execute_command(const Command& cmd) {
  if (cmd.name == "help") {
    return cmd_help(cmd);
  } else if (cmd.name == "exit" || cmd.name == "quit") {
    return cmd_exit(cmd);
  } else if (cmd.name == "info") {
    return cmd_info(cmd);
  } else if (cmd.name == "format") {
    return cmd_format(cmd);
  } else if (cmd.name == "ls") {
    return cmd_ls(cmd);
  } else if (cmd.name == "mkdir") {
    return cmd_mkdir(cmd);
  } else if (cmd.name == "touch") {
    return cmd_touch(cmd);
  } else if (cmd.name == "rm") {
    return cmd_rm(cmd);
  } else if (cmd.name == "cat") {
    return cmd_cat(cmd);
  } else if (cmd.name == "echo") {
    return cmd_echo(cmd);
  } else if (cmd.name == "copy" || cmd.name == "cp") {
    return cmd_copy(cmd);
  } else if (cmd.name == "stress") {
    return cmd_stress(cmd);
  } else {
    ErrorHandler::log_error(ERROR_UNKNOWN_COMMAND,
                            "Unknown command: " + cmd.name);
    return false;
  }
}

/**
 * @brief 获取对内部命令解析器的引用。
 * @return CommandParser& 命令解析器对象的引用。
 */
CommandParser& CLIInterface::get_parser() {
  return parser;
}

// =============================================================================
// Command Handlers
// =============================================================================

/** @brief 处理 'help' 命令。*/
bool CLIInterface::cmd_help(const Command& cmd) {
  (void)cmd;  // 抑制未使用参数的警告
  parser.show_help();
  return true;
}

/** @brief 处理 'exit' 或 'quit' 命令。*/
bool CLIInterface::cmd_exit(const Command& cmd) {
  (void)cmd;
  running = false;
  std::cout << "Goodbye!" << std::endl;
  return true;
}

/** @brief 处理 'info' 命令。*/
bool CLIInterface::cmd_info(const Command& cmd) {
  (void)cmd;
  std::string info;
  if (filesystem.get_disk_info(info)) {
    std::cout << info;
    return true;
  } else {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to get disk information");
    return false;
  }
}

/** @brief 处理 'format' 命令。*/
bool CLIInterface::cmd_format(const Command& cmd) {
  (void)cmd;
  if (filesystem.format()) {
    std::cout << "Disk formatted successfully" << std::endl;
    return true;
  } else {
    ErrorHandler::log_error(ERROR_FORMAT_FAILED, "Failed to format disk");
    return false;
  }
}

/** @brief 处理 'ls' 命令。*/
bool CLIInterface::cmd_ls(const Command& cmd) {
  std::string path = cmd.args.empty() ? "/" : cmd.args[0];
  std::string normalized_path = PathUtils::normalize_path(path);

  std::vector<DirectoryEntry> entries;
  if (!ErrorHandler::check_and_log(
          filesystem.list_directory(normalized_path, entries), ERROR_IO_ERROR,
          "Failed to list directory: " + normalized_path)) {
    return false;
  }

  // Print directory entries
  for (const DirectoryEntry& entry : entries) {
    print_directory_entry(entry);
  }
  std::cout << std::endl;
  return true;
}

/** @brief 处理 'mkdir' 命令。*/
bool CLIInterface::cmd_mkdir(const Command& cmd) {
  std::string path = cmd.args[0];
  std::string normalized_path = PathUtils::normalize_path(path);

  bool result = filesystem.create_directory(normalized_path);
  if (result) {
    std::cout << "Directory created: " << normalized_path << std::endl;
  }
  return result;
}

/** @brief 处理 'touch' 命令。*/
bool CLIInterface::cmd_touch(const Command& cmd) {
  std::string path = cmd.args[0];
  std::string normalized_path = PathUtils::normalize_path(path);

  int inode = filesystem.create_file(
      normalized_path, FILE_PERMISSION_READ | FILE_PERMISSION_WRITE);
  if (inode != -1) {
    std::cout << "File created: " << normalized_path << std::endl;
    return true;
  }
  return false;
}

/** @brief 处理 'rm' 命令。*/
bool CLIInterface::cmd_rm(const Command& cmd) {
  std::string path = cmd.args[0];
  std::string normalized_path = PathUtils::normalize_path(path);

  // FileSystem::delete_file 和 remove_directory 会处理文件/目录不存在的错误
  bool result = filesystem.delete_file(normalized_path) ||
                filesystem.remove_directory(normalized_path);
  if (result) {
    std::cout << "Removed: " << normalized_path << std::endl;
  }
  return result;
}

/** @brief 处理 'cat' 命令。*/
bool CLIInterface::cmd_cat(const Command& cmd) {
  std::string path = cmd.args[0];
  std::string normalized_path = PathUtils::normalize_path(path);

  if (!filesystem.file_exists(normalized_path)) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "File not found: " + normalized_path);
    return false;
  }

  int fd = filesystem.open_file(normalized_path, OPEN_MODE_READ);
  if (fd == -1) {
    // 错误已在 open_file 内部记录
    return false;
  }

  char buffer[1024];
  int bytes_read;
  bool read_error = false;

  while ((bytes_read = filesystem.read_file(fd, buffer, sizeof(buffer) - 1)) >
         0) {
    buffer[bytes_read] = '\0';
    std::cout << buffer;
  }

  if (bytes_read < 0) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to read file: " + normalized_path);
    read_error = true;
  }

  filesystem.close_file(fd);
  std::cout << std::endl;
  return !read_error;
}

/** @brief 处理 'echo' 命令。*/
bool CLIInterface::cmd_echo(const Command& cmd) {
  std::string path = cmd.args.back();
  std::string normalized_path = PathUtils::normalize_path(path);

  std::string text;
  for (size_t i = 0; i < cmd.args.size() - 2; i++) {
    if (i > 0)
      text += " ";
    text += cmd.args[i];
  }

  int fd =
      filesystem.open_file(normalized_path, OPEN_MODE_WRITE | OPEN_MODE_CREATE);
  if (fd == -1) {
    // 错误已在 open_file 内部记录
    return false;
  }

  int bytes_written = filesystem.write_file(fd, text.c_str(), text.length());
  filesystem.close_file(fd);

  if (bytes_written != static_cast<int>(text.length())) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to write to file: " + normalized_path);
    return false;
  } else {
    std::cout << "Written to file: " << normalized_path << std::endl;
    return true;
  }
}

/**
 * @brief 处理 'copy' 命令。
 */
bool CLIInterface::cmd_copy(const Command& cmd) {
  if (cmd.args.size() != 2) {
    ErrorHandler::log_error(
        ERROR_INVALID_ARGUMENT,
        "copy requires exactly two arguments: source and destination");
    return false;
  }

  std::string src_path = PathUtils::normalize_path(cmd.args[0]);
  std::string dst_path = PathUtils::normalize_path(cmd.args[1]);

  // 检查源文件是否存在
  if (!filesystem.file_exists(src_path)) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Source file not found: " + src_path);
    return false;
  }

  // 打开源文件
  int src_fd = filesystem.open_file(src_path, OPEN_MODE_READ);
  if (src_fd == -1) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to open source file: " + src_path);
    return false;
  }

  // 读取整个源文件到内存中
  const int buffer_size = 4096;  // 使用4KB缓冲区
  std::vector<char> file_content;
  int total_bytes_read = 0;
  int bytes_read;

  do {
    std::vector<char> buffer(buffer_size);
    bytes_read = filesystem.read_file(src_fd, buffer.data(), buffer_size);

    if (bytes_read > 0) {
      file_content.insert(file_content.end(), buffer.data(),
                          buffer.data() + bytes_read);
      total_bytes_read += bytes_read;
    }
  } while (bytes_read == buffer_size);  // 继续读取直到文件末尾

  filesystem.close_file(src_fd);

  if (bytes_read < 0) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to read source file: " + src_path);
    return false;
  }

  // 创建目标文件
  int dst_fd =
      filesystem.open_file(dst_path, OPEN_MODE_WRITE | OPEN_MODE_CREATE);
  if (dst_fd == -1) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to create destination file: " + dst_path);
    return false;
  }

  // 写入目标文件
  int bytes_written = 0;
  if (total_bytes_read > 0) {
    bytes_written =
        filesystem.write_file(dst_fd, file_content.data(), total_bytes_read);
  } else {
    // For empty files, we still need to make sure the file is created by
    // calling write_file Even with 0 bytes, it will create an empty file
    bytes_written = filesystem.write_file(dst_fd, nullptr, 0);
    if (bytes_written == 0) {
      bytes_written =
          total_bytes_read;  // To make success check pass for empty files
    }
  }
  bool success = (bytes_written == total_bytes_read);

  filesystem.close_file(dst_fd);

  if (success) {
    std::cout << "File copied from " << src_path << " to " << dst_path
              << std::endl;
  } else {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to write to destination file: " + dst_path);
  }

  return success;
}

/** @brief 处理 'stress' 命令。*/
bool CLIInterface::cmd_stress(const Command& cmd) {
  StressTestConfig config;
  std::string error_message;
  if (!parse_stress_arguments(cmd.args, config, error_message)) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT, error_message);
    return false;
  }

  StressTester tester(filesystem);
  bool success = tester.run(config);
  if (success) {
    std::cout << "[Stress] Test finished successfully" << std::endl;
  } else {
    std::cout << "[Stress] Test finished with errors" << std::endl;
  }
  return success;
}

// =============================================================================
// Private Helper Methods
// =============================================================================

/** @brief 获取命令提示符字符串。*/
std::string CLIInterface::get_prompt() const {
  return "disk-sim> ";
}

/** @brief 打印单个目录条目。*/
void CLIInterface::print_directory_entry(const DirectoryEntry& entry) {
  char name[MAX_FILENAME_LENGTH + 1];
  memset(name, 0, sizeof(name));
  int copy_len =
      std::min(static_cast<int>(entry.name_length), MAX_FILENAME_LENGTH);
  strncpy(name, entry.name, copy_len);
  name[copy_len] = '\0';

  std::cout << name;

  // 简单地为 . 和 .. 添加斜杠，其他留空
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    std::cout << "/";
  }

  std::cout << "\t";
}