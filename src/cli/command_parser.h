#pragma once
#include <string>
#include <vector>

#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"

/**
 * @class CommandParser
 * @brief 解析用户输入的命令行字符串。
 *
 * 负责将一行文本输入分割为命令名称和参数，并验证其基本合法性。
 */
class CommandParser {
 public:
  /**
   * @brief 构造函数。
   */
  CommandParser();

  /**
   * @brief 析构函数。
   */
  ~CommandParser();

  /**
   * @brief 解析一行命令行输入。
   * @param line 用户输入的完整命令行字符串。
   * @param[out] cmd 用于存储解析结果的Command结构体。
   * @return bool 解析成功且命令有效返回true，否则返回false。
   */
  bool parse_line(const std::string& line, Command& cmd);

  /**
   * @brief 显示所有可用命令的帮助信息。
   */
  void show_help() const;

 private:
  /**
   * @brief 将命令行字符串分割为参数列表。
   * @param line 要分割的命令行字符串。
   * @return std::vector<std::string> 分割后的参数向量。
   */
  std::vector<std::string> split_args(const std::string& line);

  /**
   * @brief 验证解析后的命令是否有效。
   * @param cmd 要验证的Command结构体。
   * @return bool 如果命令有效返回true，否则返回false。
   */
  bool validate_command(const Command& cmd) const;

  std::vector<std::string> supported_commands;  ///< 支持的命令列表
};