#pragma once

#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>
#include "thread_pool.h"
#include "task_wrapper.h"

class FileSystem;  // 前向声明

/**
 * @class TaskDispatcher
 * @brief 任务分发器，实现单生产者多消费者模式。
 * 
 * 负责接收任务并分发到线程池中执行。
 * 通过改进的同步机制提升并发性能。
 */
class TaskDispatcher {
public:
    /**
     * @brief 构造函数。
     * @param fs 文件系统对象的引用。
     * @param num_threads 线程池中的线程数量。
     */
    TaskDispatcher(FileSystem& fs, size_t num_threads = 4);

    /**
     * @brief 析构函数。
     */
    ~TaskDispatcher();

    /**
     * @brief 异步执行一个CLI命令。
     * @param command_line 要执行的命令行字符串。
     * @return std::future<int> 用于获取执行结果的future对象。
     */
    std::future<int> execute_async(const std::string& command_line);

    /**
     * @brief 同步执行一个CLI命令。
     * @param command_line 要执行的命令行字符串。
     * @return int 执行结果（0表示成功）。
     */
    int execute_sync(const std::string& command_line);

    /**
     * @brief 获取线程池大小。
     * @return size_t 线程池中的线程数量。
     */
    size_t get_thread_count() const;

private:
    enum class DispatchMode {
        Shared,
        Exclusive
    };

    [[nodiscard]] DispatchMode resolve_mode(const std::string& command_line) const;
    [[nodiscard]] std::string extract_command_name(const std::string& command_line) const;

    std::unique_ptr<ThreadPool> thread_pool_;   ///< 线程池
    FileSystem& filesystem_;                    ///< 文件系统引用
    mutable std::shared_mutex dispatcher_mutex_;///< 调度级别的读写锁
};