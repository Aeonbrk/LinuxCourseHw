// ==============================================================================
// @file   task_dispatcher.cpp
// @brief  任务分发器的实现
// ==============================================================================

#include "task_dispatcher.h"
#include "../core/filesystem.h"
#include "task_wrapper.h"
#include <shared_mutex>

/**
 * @brief 构造函数。
 * @param fs 文件系统对象的引用。
 * @param num_threads 线程池中的线程数量。
 */
TaskDispatcher::TaskDispatcher(FileSystem& fs, size_t num_threads) 
    : thread_pool_(std::make_unique<ThreadPool>(num_threads)), 
      filesystem_(fs) {
}

/**
 * @brief 析构函数。
 */
TaskDispatcher::~TaskDispatcher() = default;

/**
 * @brief 异步执行一个CLI命令。
 * @param command_line 要执行的命令行字符串。
 * @return std::future<int> 用于获取执行结果的future对象。
 */
std::future<int> TaskDispatcher::execute_async(const std::string& command_line) {
    DispatchMode mode = resolve_mode(command_line);

    auto task = [this, command_line, mode]() -> int {
        if (mode == DispatchMode::Shared) {
            std::shared_lock<std::shared_mutex> lock(dispatcher_mutex_);
            return TaskWrapper::execute_command_line(filesystem_, command_line);
        }

        std::unique_lock<std::shared_mutex> lock(dispatcher_mutex_);
        return TaskWrapper::execute_command_line(filesystem_, command_line);
    };

    return thread_pool_->enqueue(task);
}

/**
 * @brief 同步执行一个CLI命令。
 * @param command_line 要执行的命令行字符串。
 * @return int 执行结果（0表示成功）。
 */
int TaskDispatcher::execute_sync(const std::string& command_line) {
    DispatchMode mode = resolve_mode(command_line);

    if (mode == DispatchMode::Shared) {
        std::shared_lock<std::shared_mutex> lock(dispatcher_mutex_);
        return TaskWrapper::execute_command_line(filesystem_, command_line);
    }

    std::unique_lock<std::shared_mutex> lock(dispatcher_mutex_);
    return TaskWrapper::execute_command_line(filesystem_, command_line);
}

/**
 * @brief 获取线程池大小。
 * @return size_t 线程池中的线程数量。
 */
size_t TaskDispatcher::get_thread_count() const {
    return thread_pool_->get_thread_count();
}

TaskDispatcher::DispatchMode TaskDispatcher::resolve_mode(const std::string& command_line) const {
    static const std::unordered_set<std::string> shared_commands = {
        "ls",
        "cat",
        "info"
    };

    std::string command = extract_command_name(command_line);
    if (command.empty()) {
        return DispatchMode::Exclusive;
    }

    return shared_commands.count(command) > 0 ? DispatchMode::Shared
                                              : DispatchMode::Exclusive;
}

std::string TaskDispatcher::extract_command_name(const std::string& command_line) const {
    size_t start = command_line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }

    size_t end = command_line.find_first_of(" \t\r\n", start);
    if (end == std::string::npos) {
        return command_line.substr(start);
    }

    return command_line.substr(start, end - start);
}