#pragma once
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/block_utils.h"
#include "../utils/error_handler.h"
#include "bitmap_manager.h"
#include <vector>

class DiskSimulator;  // 前向声明

/**
 * @class InodeManager
 * @brief 管理inode的分配、释放、读写以及其关联的数据块。
 */
class InodeManager {
 public:
  /**
   * @brief 构造函数。
   * @param disk DiskSimulator对象的引用。
   */
  InodeManager(DiskSimulator& disk);

  /**
   * @brief 析构函数。
   */
  ~InodeManager();

  /**
   * @brief 初始化Inode管理器。
   * @param layout 磁盘布局信息。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool initialize(const DiskLayout& layout);

  /**
   * @brief 分配一个空闲的inode。
   * @param[out] inode_num 分配到的inode号。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool allocate_inode(int& inode_num);

  /**
   * @brief 释放一个inode及其关联的所有数据块。
   * @param inode_num 要释放的inode号。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool free_inode(int inode_num);

  /**
   * @brief 从磁盘读取指定的inode信息。
   * @param inode_num 要读取的inode号。
   * @param[out] inode 用于存储读取信息的Inode结构体。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool read_inode(int inode_num, Inode& inode);

  /**
   * @brief 将inode信息写入磁盘。
   * @param inode_num 要写入的inode号。
   * @param inode 包含要写入信息的Inode结构体。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool write_inode(int inode_num, const Inode& inode);

  /**
   * @brief 为指定的inode分配数据块。
   * @param inode_num 目标inode号。
   * @param block_count 需要分配的块数量。
   * @param[out] block_nums 存储分配到的数据块号的向量。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool allocate_data_blocks(int inode_num, int block_count,
                            std::vector<int>& block_nums);

  /**
   * @brief 释放指定inode的所有数据块。
   * @param inode_num 目标inode号。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool free_data_blocks(int inode_num);

  /**
   * @brief 获取指定inode的所有数据块列表。
   * @param inode_num 目标inode号。
   * @param[out] block_nums 存储数据块号的向量。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool get_data_blocks(int inode_num, std::vector<int>& block_nums);

  /**
   * @brief 检查指定的inode是否已分配。
   * @param inode_num 要检查的inode号。
   * @return bool 如果已分配返回true，否则返回false。
   */
  bool is_inode_allocated(int inode_num) const;

  /** @brief 获取总inode数量。 */
  int get_total_inodes() const;

  /** @brief 获取空闲inode数量。 */
  int get_free_inodes() const;

  /** @brief 获取空闲数据块数量。 */
  int get_free_data_blocks() const;

  /**
   * @brief 重新加载位图（通常在格式化后使用）。
   * @return bool 操作成功返回true，否则返回false。
   */
  bool reload_bitmap();

 private:
  DiskSimulator& disk;          ///< 磁盘模拟器引用
  BitmapManager* inode_bitmap;  ///< inode位图管理器
  BitmapManager* data_bitmap;   ///< 数据块位图管理器
  DiskLayout layout;            ///< 磁盘布局
  bool initialized;             ///< 初始化标志

  // --- 私有辅助函数 ---
  bool check_initialized(const std::string& operation_name) const;
  bool load_bitmaps();
  bool load_inode_bitmap();
  bool save_inode_bitmap();
  bool load_data_bitmap();
  bool save_data_bitmap();
  void initialize_new_inode(Inode& inode);
  void free_all_data_blocks_for_inode(Inode& inode);
  bool read_indirect_block(int block_num, std::vector<int>& data_blocks);
  bool write_indirect_block(int block_num, const std::vector<int>& data_blocks);
  bool allocate_indirect_block(int& block_num);
  bool free_indirect_block(int block_num);
  bool allocate_single_block(uint32_t& block_index);
  bool allocate_multiple_blocks(size_t count, std::vector<uint32_t>& block_indices);
  bool update_inode_block_pointers(uint32_t inode_id, const std::vector<uint32_t>& block_indices);
  bool get_inode_position(int inode_num, int& block_num, int& offset_in_block) const;

  // --- 线程同步 ---
  mutable std::mutex inode_mutex_;    ///< 保护inode操作的互斥锁
  mutable std::mutex bitmap_mutex_;   ///< 保护位图操作的互斥锁
};