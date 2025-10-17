#pragma once
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/block_utils.h"
#include "../utils/error_handler.h"
#include <mutex>

class DiskSimulator;  // 前向声明

/**
 * @class BitmapManager
 * @brief 管理位图，用于跟踪inode和数据块等资源的分配情况。
 */
class BitmapManager {
 public:
  /**
   * @brief 构造函数。
   * @param size 位图管理的总资源数量（位数）。
   */
  BitmapManager(int size);

  /**
   * @brief 析构函数。
   */
  ~BitmapManager();

  /**
   * @brief 分配一个空闲位。
   * @param[out] bit_num 分配到的位号。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool allocate_bit(int& bit_num);

  /**
   * @brief 释放一个指定的位。
   * @param bit_num 要释放的位号。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool free_bit(int bit_num);

  /**
   * @brief 检查指定的位是否已分配。
   * @param bit_num 要检查的位号。
   * @return bool 如果已分配返回true，否则返回false。
   */
  bool is_allocated(int bit_num) const;

  /**
   * @brief 清除所有位，将整个位图标记为未分配。
   */
  void clear_all();

  /**
   * @brief 从磁盘加载位图数据。
   * @param disk DiskSimulator对象的引用。
   * @param start_block 位图在磁盘上的起始块号。
   * @param block_count 位图占用的块数。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool load_from_disk(DiskSimulator& disk, int start_block, int block_count);

  /**
   * @brief 将位图数据保存到磁盘。
   * @param disk DiskSimulator对象的引用。
   * @param start_block 位图在磁盘上的起始块号。
   * @param block_count 位图占用的块数。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool save_to_disk(DiskSimulator& disk, int start_block, int block_count);

  /**
   * @brief 获取总位数。
   * @return int 位图管理的总位数。
   */
  int get_total_bits() const;

  /**
   * @brief 获取空闲位数（O(1)复杂度）。
   * @return int 当前空闲的位数。
   */
  int get_free_bits() const;

  /**
   * @brief 获取已用位数（O(1)复杂度）。
   * @return int 当前已使用的位数。
   */
  int get_used_bits() const;

 private:
  char* bitmap_data;      ///< 位图数据指针
  int bitmap_size;        ///< 位图大小（字节）
  int total_bits;         ///< 总位数
  int free_bits_count;    ///< 空闲位数缓存，用于O(1)查询
  int start_block;        ///< 在磁盘上的起始块
  int block_count;        ///< 占用的块数

  // --- 线程同步 ---
  mutable std::mutex bitmap_mutex_;    ///< 保护位图操作的互斥锁

  // --- 私有辅助函数 ---
  bool check_initialized(const std::string& operation_name) const;
  bool is_valid_bit(int bit_num) const;
  void get_bit_location(int bit_num, int& byte_index, int& bit_offset) const;
  void set_bit(int bit_num);
  void clear_bit(int bit_num);
  int find_free_bit() const;
  void recalculate_free_bits();
};