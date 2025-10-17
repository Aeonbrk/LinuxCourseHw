#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/**
 * @class ThreadPool
 * @brief 线程池实现，用于管理工作线程。
 *
 * 此类实现单生产者多消费者模式，
 * 其中一个线程可以提交任务给多个工作线程执行。
 */
class ThreadPool {
 public:
  /**
   * @brief 构造一个新的ThreadPool对象。
   *
   * @param num_threads 要创建的工作线程数（默认为CPU核心数）。
   */
  explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());

  /**
   * @brief 销毁ThreadPool对象。
   *
   * 在销毁池之前将等待所有任务完成。
   */
  ~ThreadPool();

  /**
   * @brief 向线程池提交任务以执行。
   *
   * @param f 要执行的函数。
   * @param args 函数的参数。
   * @return std::future<typename std::result_of<F(Args...)>::type>
   * 用于检索任务结果的future对象。
   */
  template <class F, class... Args>
  auto enqueue(F&& f, Args&&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

  /**
   * @brief 获取池中工作线程的数量。
   *
   * @return size_t 线程数。
   */
  size_t get_thread_count() const;

  /**
   * @brief 获取当前在队列中的任务数。
   *
   * @return size_t 队列中的任务数。
   */
  size_t get_queue_size() const;

 private:
  // 工作线程
  std::vector<std::thread> workers;

  // 任务队列
  std::queue<std::function<void()>> tasks;

  // 同步原语
  mutable std::mutex queue_mutex;  // mutable 允许在 const 方法中修改
  std::condition_variable condition;
  std::atomic<bool> stop;
};

// 构造函数实现
inline ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
  if (num_threads == 0) {
    num_threads = std::thread::hardware_concurrency();
  }

  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([this] {
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [this] { return stop || !tasks.empty(); });

          if (stop && tasks.empty()) {
            return;
          }

          task = std::move(tasks.front());
          tasks.pop();
        }

        task();
      }
    });
  }
}

// 析构函数实现
inline ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }

  condition.notify_all();

  for (std::thread& worker : workers) {
    worker.join();
  }
}

// 入队实现
template <class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();

  {
    std::unique_lock<std::mutex> lock(queue_mutex);

    if (stop) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    tasks.emplace([task]() { (*task)(); });
  }

  condition.notify_one();
  return res;
}

// 获取线程数实现
inline size_t ThreadPool::get_thread_count() const {
  return workers.size();
}

// 获取队列大小实现
inline size_t ThreadPool::get_queue_size() const {
  std::unique_lock<std::mutex> lock(const_cast<std::mutex&>(queue_mutex));
  return tasks.size();
}