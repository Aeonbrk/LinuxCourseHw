// ==============================================================================
// @file   block_manager.h
// @brief  块管理模块，处理底层块操作
// ==============================================================================

#pragma once
#include <string>
#include <vector>

#include "../utils/block_utils.h"
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/file_operations_utils.h"
#include "disk_simulator.h"
#include "inode_manager.h"

/**
 * @class BlockManager
 * @brief 块管理器，负责处理底层块操作。
 */
class BlockManager {
 public:
  /**
   * @brief 构造函数。
   * @param disk DiskSimulator对象的引用。
   * @param inode_manager InodeManager对象的引用。
   */
  BlockManager(DiskSimulator& disk, InodeManager& inode_manager);

  /**
   * @brief 从数据块读取数据到缓冲区。
   * @param blocks 数据块列表。
   * @param offset 偏移量。
   * @param buffer 输出缓冲区。
   * @param size 要读取的大小。
   * @return bool 读取成功返回true，否则返回false。
   */
  bool read_data_from_blocks(const std::vector<int>& blocks, int offset,
                             char* buffer, int size);

  /**
   * @brief 将缓冲区数据写入到数据块。
   * @param blocks 数据块列表。
   * @param offset 偏移量。
   * @param buffer 输入缓冲区。
   * @param size 要写入的大小。
   * @return bool 写入成功返回true，否则返回false。
   */
  bool write_data_to_blocks(const std::vector<int>& blocks, int offset,
                            const char* buffer, int size);

  /**
   * @brief 分配文件数据块。
   * @param size 文件大小。
   * @param block_indices 用于存储分配到的块索引的向量。
   * @return bool 分配成功返回true，否则返回false。
   */
  bool allocate_file_data_blocks(size_t size,
                                 std::vector<uint32_t>& block_indices);

  /**
   * @brief 写入文件数据到分配的块。
   * @param inode_id inode ID。
   * @param data 要写入的数据。
   * @param size 数据大小。
   * @param block_indices 块索引向量。
   * @return bool 写入成功返回true，否则返回false。
   */
  bool write_file_data(uint32_t inode_id, const char* data, size_t size,
                       const std::vector<uint32_t>& block_indices);

 private:
  DiskSimulator& disk;          ///< 磁盘模拟器引用
  InodeManager& inode_manager;  ///< Inode管理器引用
};