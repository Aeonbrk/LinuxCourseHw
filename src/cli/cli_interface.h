#pragma once
#include <string>

#include "../core/filesystem.h"
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/path_utils.h"
#include "command_parser.h"

/**
 * @class CLIInterface
 * @brief 提供用户交互的命令行界面。
 *
 * 负责运行主循环，接收用户输入，调用CommandParser进行解析，
 * 并执行相应的FileSystem操作。
 */
class CLIInterface {
 public:
  /**
   * @brief 构造函数。
   * @param fs FileSystem对象的引用。
   */
  CLIInterface(FileSystem& fs);

  /**
   * @brief 析构函数。
   */
  ~CLIInterface();

  /**
   * @brief 运行命令行界面主循环。
   */
  void run();

  /**
   * @brief 执行一个已解析的命令。
   * @param cmd 要执行的Command对象。
   * @return bool 命令是否成功执行。
   */
  bool execute_command(const Command& cmd);

  /**
   * @brief 获取对内部命令解析器的引用。
   * @return CommandParser& 命令解析器对象的引用。
   */
  CommandParser& get_parser();

 private:
  FileSystem& filesystem;  ///< 文件系统引用
  CommandParser parser;    ///< 命令解析器实例
  bool running;            ///< 运行状态标志

  // 命令行辅助函数
  bool cmd_help(const Command& cmd);
  bool cmd_exit(const Command& cmd);
  bool cmd_info(const Command& cmd);
  bool cmd_format(const Command& cmd);
  bool cmd_ls(const Command& cmd);
  bool cmd_mkdir(const Command& cmd);
  bool cmd_touch(const Command& cmd);
  bool cmd_rm(const Command& cmd);
  bool cmd_cat(const Command& cmd);
  bool cmd_echo(const Command& cmd);
  bool cmd_copy(const Command& cmd);
  bool cmd_stress(const Command& cmd);

  // UI 辅助函数
  std::string get_prompt() const;
  void print_directory_entry(const DirectoryEntry& entry);
};