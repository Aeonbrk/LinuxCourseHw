#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

/**
 * @brief 用于单生产者多消费者场景的线程安全队列。
 *
 * 此模板类提供了一个线程安全队列，可以安全地在线程之间传递
 * 任务或数据。
 *
 * @tparam T 队列中存储的元素类型。
 */
template <typename T>
class TaskQueue {
 public:
  /**
   * @brief 构造一个新的TaskQueue对象。
   */
  TaskQueue() : finished(false) {}

  /**
   * @brief 销毁TaskQueue对象。
   */
  ~TaskQueue() = default;

  /**
   * @brief 向队列中添加一个项目。
   *
   * @param item 要添加到队列的项目。
   */
  void push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (finished) {
      throw std::runtime_error("Cannot push to a finished queue");
    }
    queue_.push(std::move(item));
    condition_.notify_one();
  }

  /**
   * @brief 尝试从队列中弹出一个项目。
   *
   * @param item 用于存储弹出项目的引用。
   * @return true 如果项目成功弹出。
   * @return false 如果队列为空且未完成。
   */
  bool try_pop(T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    item = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  /**
   * @brief 等待并从队列中弹出一个项目。
   *
   * @param item 用于存储弹出项目的引用。
   * @return true 如果项目成功弹出。
   * @return false 如果队列已完成且为空。
   */
  bool wait_and_pop(T& item) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty() || finished; });

    if (finished && queue_.empty()) {
      return false;
    }

    item = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  /**
   * @brief 标记队列已完成。
   *
   * 这将取消任何等待的消费者阻塞并阻止添加新项目。
   */
  void finish() {
    std::unique_lock<std::mutex> lock(mutex_);
    finished = true;
    condition_.notify_all();
  }

  /**
   * @brief 检查队列是否为空。
   *
   * @return true 如果队列为空。
   * @return false 否则。
   */
  bool empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  /**
   * @brief 获取队列的大小。
   *
   * @return size_t 队列中的项目数。
   */
  size_t size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  mutable std::mutex mutex_;           // 用于线程安全的互斥锁
  std::queue<T> queue_;                // 任务的实际队列
  std::condition_variable condition_;  // 用于等待的条件变量
  bool finished;                       // 指示队列是否已完成的标志
};