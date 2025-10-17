#pragma once

#include <functional>
#include <string>
#include <memory>

class FileSystem;  // 前向声明

/**
 * @class TaskWrapper
 * @brief 封装CLI命令执行任务的类。
 * 
 * 用于将CLI命令封装为可以在多线程环境中执行的任务。
 */
class TaskWrapper {
public:
    /**
     * @brief 构造函数。
     */
    TaskWrapper() = default;

    /**
     * @brief 执行一个命令。
     * @param fs 文件系统对象的引用。
     * @param cmd 要执行的命令。
     * @return int 执行结果（0表示成功）。
     */
    static int execute_command(FileSystem& fs, const std::string& command);

    /**
     * @brief 执行一个命令字符串。
     * @param fs 文件系统对象的引用。
     * @param command_line 要执行的命令行字符串。
     * @return int 执行结果（0表示成功）。
     */
    static int execute_command_line(FileSystem& fs, const std::string& command_line);
};