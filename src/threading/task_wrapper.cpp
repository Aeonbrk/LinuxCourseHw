// ==============================================================================
// @file   task_wrapper.cpp
// @brief  封装CLI命令执行任务的类的实现
// ==============================================================================

#include "task_wrapper.h"
#include "../cli/cli_interface.h"
#include "../cli/command_parser.h"

/**
 * @brief 执行一个命令。
 * @param fs 文件系统对象的引用。
 * @param cmd 命令字符串。
 * @return int 执行结果（0表示成功）。
 */
int TaskWrapper::execute_command(FileSystem& fs, const std::string& command) {
    CLIInterface cli(fs);
    Command cmd;
    
    if (!cli.get_parser().parse_line(command, cmd)) {
        return 1; // 解析失败
    }
    
    return cli.execute_command(cmd) ? 0 : 1;
}

/**
 * @brief 执行一个命令字符串。
 * @param fs 文件系统对象的引用。
 * @param command_line 要执行的命令行字符串。
 * @return int 执行结果（0表示成功）。
 */
int TaskWrapper::execute_command_line(FileSystem& fs, const std::string& command_line) {
    return execute_command(fs, command_line);
}