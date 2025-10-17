#pragma once
#include <cstdio>
#include <string>
#include <mutex>

#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/block_utils.h"

/**
 * @class DiskSimulator
 * @brief 模拟磁盘，提供块级的原子读写操作。
 *
 * 通过一个大文件模拟物理磁盘，并负责处理磁盘的创建、打开、格式化以及块数据的读写。
 */
class DiskSimulator {
 public:
  /**
   * @brief 构造函数。
   */
  DiskSimulator();

  /**
   * @brief 析构函数，确保磁盘文件被关闭。
   */
  ~DiskSimulator();

  /**
   * @brief 创建一个新的磁盘文件。
   * @param path 要创建的磁盘文件的路径。
   * @param size_mb 磁盘文件的大小（以MB为单位）。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool create_disk(const std::string& path, int size_mb);

  /**
   * @brief 打开一个已存在的磁盘文件。
   * @param path 要打开的磁盘文件的路径。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool open_disk(const std::string& path);

  /**
   * @brief 从磁盘读取一个数据块。
   * @param block_num 要读取的块号。
   * @param buffer 用于存储读取数据的缓冲区。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool read_block(int block_num, char* buffer);

  /**
   * @brief 向磁盘写入一个数据块。
   * @param block_num 要写入的块号。
   * @param buffer 包含要写入数据的缓冲区。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool write_block(int block_num, const char* buffer);

  /**
   * @brief 格式化磁盘，创建文件系统结构。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool format_disk();

  /**
   * @brief 关闭磁盘文件。
   */
  void close_disk();

  /**
   * @brief 检查磁盘是否已打开。
   * @return bool 如果磁盘已打开返回true，否则返回false。
   */
  bool is_open() const;

  /**
   * @brief 获取磁盘总块数。
   * @return int 磁盘的总块数。
   */
  int get_total_blocks() const;

  /**
   * @brief 获取磁盘大小（字节）。
   * @return long 磁盘的总大小（字节）。
   */
  long get_disk_size() const;

  /**
   * @brief 获取块大小（字节）。
   * @return int 单个块的大小（字节）。
   */
  int get_block_size() const;

  /**
   * @brief 获取磁盘文件路径。
   * @return std::string 磁盘文件的路径。
   */
  std::string get_disk_path() const;

  /**
   * @brief 计算并返回磁盘布局。
   * @return DiskLayout 包含文件系统各部分布局信息的结构体。
   */
  DiskLayout calculate_layout() const;

 private:
  std::string disk_path;  ///< 磁盘文件路径
  FILE* disk_file;        ///< 磁盘文件指针
  long disk_size;         ///< 磁盘大小（字节）
  int total_blocks;       ///< 磁盘总块数
  bool disk_open;         ///< 磁盘是否打开标志
  bool lock_acquired;     ///< 是否持有跨进程锁

  // --- 线程同步 ---
  mutable std::mutex disk_mutex_;    ///< 保护磁盘文件操作的互斥锁

  // --- 私有辅助函数 ---
  bool is_ready_for_io(int block_num) const;
  bool seek_to_block(int block_num);
  bool initialize_superblock(const DiskLayout& layout);
  bool initialize_bitmaps(const DiskLayout& layout);
  bool initialize_inode_table(const DiskLayout& layout);
  bool write_zeroed_blocks(int start_block, int num_blocks);
};