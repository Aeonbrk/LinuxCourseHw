// ==============================================================================
// @file   block_manager.cpp
// @brief  块管理模块的实现
// ==============================================================================

#include "block_manager.h"

#include <cstring>
#include <iostream>

// ==============================================================================
// 构造与析构
// ==============================================================================

/**
 * @brief 构造函数。
 * @param disk DiskSimulator对象的引用。
 * @param inode_manager InodeManager对象的引用。
 */
BlockManager::BlockManager(DiskSimulator& disk, InodeManager& inode_manager)
    : disk(disk), inode_manager(inode_manager) {
}

// ==============================================================================
// 公共接口方法
// ==============================================================================

/**
 * @brief 从数据块读取数据到缓冲区。
 * @param blocks 数据块列表。
 * @param offset 偏移量。
 * @param buffer 输出缓冲区。
 * @param size 要读取的大小。
 * @return bool 读取成功返回true，否则返回false。
 */
bool BlockManager::read_data_from_blocks(const std::vector<int>& blocks,
                                         int offset, char* buffer, int size) {
  return FileOperationsUtils::read_data_from_blocks(disk, blocks, offset, buffer, size);
}

/**
 * @brief 将缓冲区数据写入到数据块。
 * @param blocks 数据块列表。
 * @param offset 偏移量。
 * @param buffer 输入缓冲区。
 * @param size 要写入的大小。
 * @return bool 写入成功返回true，否则返回false。
 */
bool BlockManager::write_data_to_blocks(const std::vector<int>& blocks,
                                        int offset, const char* buffer,
                                        int size) {
  return FileOperationsUtils::write_data_to_blocks(disk, blocks, offset, buffer, size);
}

/**
 * @brief 分配文件数据块。
 * @param size 文件大小。
 * @param block_indices 用于存储分配到的块索引的向量。
 * @return bool 分配成功返回true，否则返回false。
 */
bool BlockManager::allocate_file_data_blocks(
    size_t size, std::vector<uint32_t>& block_indices) {
  uint32_t blocks_needed = BlockUtils::calculate_blocks_needed(size);

  if (blocks_needed == 0) {
    return true;
  }

  std::vector<int> allocated_blocks;
  if (!inode_manager.allocate_data_blocks(-1, blocks_needed,
                                          allocated_blocks)) {
    ErrorHandler::log_error(ERROR_NO_FREE_BLOCKS,
                            "Failed to allocate data blocks");
    return false;
  }

  block_indices.clear();
  for (int block : allocated_blocks) {
    if (!BlockUtils::is_valid_block_index(static_cast<uint32_t>(block))) {
      ErrorHandler::log_error(
          ERROR_INVALID_BLOCK,
          "Invalid block index allocated: " + std::to_string(block));
      return false;
    }
    block_indices.push_back(static_cast<uint32_t>(block));
  }

  return true;
}

/**
 * @brief 写入文件数据到分配的块。
 * @param inode_id inode ID。
 * @param data 要写入的数据。
 * @param size 数据大小。
 * @param block_indices 块索引向量。
 * @return bool 写入成功返回true，否则返回false。
 */
bool BlockManager::write_file_data(uint32_t inode_id, const char* data,
                                   size_t size,
                                   const std::vector<uint32_t>& block_indices) {
  if (block_indices.empty() && size > 0) {
    ErrorHandler::log_error(ERROR_INVALID_BLOCK,
                            "No blocks allocated for non-empty file");
    return false;
  }

  if (size == 0) {
    return true;
  }

  size_t bytes_written = 0;
  for (size_t i = 0; i < block_indices.size() && bytes_written < size; i++) {
    auto block_buffer = BlockUtils::create_block_buffer();

    size_t write_size =
        std::min(static_cast<size_t>(BLOCK_SIZE), size - bytes_written);

    if (!BlockUtils::copy_block_data(block_buffer.get(), data + bytes_written,
                                     write_size)) {
      ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                              "Failed to copy data to block buffer");
      return false;
    }

    if (!disk.write_block(block_indices[i], block_buffer.get())) {
      ErrorHandler::log_error(
          ERROR_IO_ERROR,
          "Failed to write block " + std::to_string(block_indices[i]));
      return false;
    }

    bytes_written += write_size;
  }

  Inode inode;
  if (!inode_manager.read_inode(inode_id, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read inode for update");
    return false;
  }

  inode.size = size;
  inode.modification_time = time(nullptr);

  if (!inode_manager.write_inode(inode_id, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to update inode with new size");
    return false;
  }

  return true;
}