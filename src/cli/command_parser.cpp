// ==============================================================================
// @file   command_parser.cpp
// @brief  用户输入命令解析器的实现
// ==============================================================================

#include "command_parser.h"

#include <algorithm>
#include <iostream>
#include <sstream>

/**
 * @brief 构造函数，初始化命令解析器。
 */
CommandParser::CommandParser() {
  // 初始化支持的命令列表
  supported_commands = {"help",  "exit",    "quit",   "info",  "format",
                        "ls",    "mkdir",   "touch",  "rm",    "cat",
                        "echo",  "copy",    "stress"};
}

/**
 * @brief 析构函数。
 */
CommandParser::~CommandParser() {
}

/**
 * @brief 解析一行输入的命令字符串。
 *
 * @param line 用户输入的命令行字符串。
 * @param[out] cmd 解析结果将存储在该Command结构体中。
 * @return bool 解析成功返回true，否则返回false。
 */
bool CommandParser::parse_line(const std::string& line, Command& cmd) {
  if (line.empty()) {
    return false;
  }

  std::vector<std::string> args = split_args(line);
  if (args.empty()) {
    return false;
  }

  cmd.name = args[0];

  if (args.size() > 1) {
    cmd.args.assign(args.begin() + 1, args.end());
  } else {
    cmd.args.clear();
  }

  return validate_command(cmd);
}

/**
 * @brief 显示所有可用命令的帮助信息。
 */
void CommandParser::show_help() const {
  std::cout << "Available commands:" << std::endl;
  std::cout << "  help              - Show this help message" << std::endl;
  std::cout << "  exit, quit        - Exit the program" << std::endl;
  std::cout << "  info              - Show disk information" << std::endl;
  std::cout << "  format            - Format the disk" << std::endl;
  std::cout << "  ls [path]         - List directory contents" << std::endl;
  std::cout << "  mkdir <path>      - Create a directory" << std::endl;
  std::cout << "  touch <path>      - Create an empty file" << std::endl;
  std::cout << "  rm <path>         - Remove a file or directory" << std::endl;
  std::cout << "  cat <path>        - Display file contents" << std::endl;
  std::cout << "  echo <text> > <path> - Write text to a file" << std::endl;
  std::cout << "  copy <src> <dst>  - Copy a file from source to destination"
            << std::endl;
  std::cout << "  stress [options] - Run storage stress workload" << std::endl;
  std::cout << std::endl;
}

/**
 * @brief 将命令行字符串分割为参数列表。
 *
 * @param line 要分割的命令行字符串。
 * @return std::vector<std::string> 分割后的参数向量。
 */
std::vector<std::string> CommandParser::split_args(const std::string& line) {
  std::vector<std::string> args;
  std::stringstream ss(line);
  std::string arg;

  // 使用 stringstream 按空格分割参数
  while (ss >> arg) {
    args.push_back(arg);
  }

  return args;
}

/**
 * @brief 验证解析后的命令是否有效。
 *
 * @param cmd 要验证的Command结构体。
 * @return bool 如果命令有效（包括命令名和参数数量）返回true，否则返回false。
 */
bool CommandParser::validate_command(const Command& cmd) const {
  // 检查命令是否在支持列表中
  if (std::find(supported_commands.begin(), supported_commands.end(),
                cmd.name) == supported_commands.end()) {
    ErrorHandler::log_error(ERROR_UNKNOWN_COMMAND,
                            "Unknown command: " + cmd.name);
    return false;
  }

  // 检查特定命令的参数数量
  if (cmd.name == "mkdir" || cmd.name == "touch" || cmd.name == "rm" ||
      cmd.name == "cat") {
    if (cmd.args.size() != 1) {
      ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                              cmd.name + " requires exactly one argument");
      return false;
    }
  } else if (cmd.name == "echo") {
    if (cmd.args.size() < 3 || cmd.args[cmd.args.size() - 2] != ">") {
      ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                              "Usage: echo <text> > <path>");
      return false;
    }
  } else if (cmd.name == "copy" || cmd.name == "cp") {
    if (cmd.args.size() != 2) {
      ErrorHandler::log_error(
          ERROR_INVALID_ARGUMENT,
          "copy requires exactly two arguments: source and destination");
      return false;
    }
  }

  return true;
}
