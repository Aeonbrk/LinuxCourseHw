// ==============================================================================
// @file   bitmap_manager.cpp
// @brief  位图管理器的实现，用于跟踪资源分配
// ==============================================================================

#include "bitmap_manager.h"
#include "disk_simulator.h"
#include <cstring>
#include <iostream>
#include <algorithm> // for std::min

// ==============================================================================
// 构造与析构
// ==============================================================================

/**
 * @brief 构造函数，初始化位图。
 * @param size 位图能够管理的总资源数量（即总位数）。
 */
BitmapManager::BitmapManager(int size)
    : bitmap_data(nullptr),
      bitmap_size(0),
      total_bits(size),
      free_bits_count(size),
      start_block(0),
      block_count(0) {
  if (total_bits > 0) {
    // 向上取整计算存储位图所需的字节数
    bitmap_size = (total_bits + 7) / 8;
    try {
      bitmap_data = new char[bitmap_size];
      // 初始化所有位为0（空闲）
      BlockUtils::clear_buffer(bitmap_data, bitmap_size);
    } catch (const std::bad_alloc& e) {
      ErrorHandler::log_error(ErrorCode::ERROR_OUT_OF_MEMORY, "Failed to allocate memory for bitmap");
      bitmap_data = nullptr;
      bitmap_size = 0;
      total_bits = 0;
      free_bits_count = 0;
    }
  }
}

/**
 * @brief 析构函数，释放位图数据所占用的内存。
 */
BitmapManager::~BitmapManager() {
  delete[] bitmap_data;
  bitmap_data = nullptr;
}

// ==============================================================================
// 公共接口方法
// ==============================================================================

/**
 * @brief 分配一个空闲位，并将其标记为已使用。
 * @param[out] bit_num 如果找到空闲位，则该参数被设置为分配到的位号。
 * @return bool 成功分配返回true，没有空闲位则返回false。
 */
bool BitmapManager::allocate_bit(int& bit_num) {
  if (!check_initialized("allocate_bit")) return false;

  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  bit_num = find_free_bit();
  if (bit_num == -1) {
    ErrorHandler::log_error(ErrorCode::ERROR_NO_FREE_BLOCKS, "No free bit available in bitmap");
    return false;
  }

  set_bit(bit_num);
  free_bits_count--;
  return true;
}

/**
 * @brief 释放一个指定的位，将其标记为空闲。
 * @param bit_num 要释放的位号。
 * @return bool 成功释放返回true，位号无效则返回false。
 */
bool BitmapManager::free_bit(int bit_num) {
  if (!check_initialized("free_bit") || !is_valid_bit(bit_num)) {
    ErrorHandler::log_error(ErrorCode::ERROR_INVALID_ARGUMENT, "Invalid bit number to free: " + std::to_string(bit_num));
    return false;
  }

  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  int byte_index, bit_offset;
  get_bit_location(bit_num, byte_index, bit_offset);
  bool was_allocated = (bitmap_data[byte_index] & (1 << bit_offset)) != 0;

  if (was_allocated) {
    clear_bit(bit_num);
    free_bits_count++;
  }

  return true;
}

/**
 * @brief 检查指定的位是否已被分配。
 * @param bit_num 要检查的位号。
 * @return bool 如果已分配返回true，否则返回false。
 */
bool BitmapManager::is_allocated(int bit_num) const {
  if (!is_valid_bit(bit_num)) {
    return false; // 无效位号视为未分配
  }

  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  int byte_index, bit_offset;
  get_bit_location(bit_num, byte_index, bit_offset);

  return (bitmap_data[byte_index] & (1 << bit_offset)) != 0;
}

/**
 * @brief 清空所有位，将整个位图重置为未分配状态。
 */
void BitmapManager::clear_all() {
  if (check_initialized("clear_all")) {
    std::lock_guard<std::mutex> lock(bitmap_mutex_);
    BlockUtils::clear_buffer(bitmap_data, bitmap_size);
    free_bits_count = total_bits;
  }
}

/**
 * @brief 从磁盘加载位图数据。
 * @param disk DiskSimulator对象的引用。
 * @param start_block_num 位图在磁盘上的起始块号。
 * @param num_blocks 位图占用的块数。
 * @return bool 成功加载返回true，否则返回false。
 */
bool BitmapManager::load_from_disk(DiskSimulator& disk, int start_block_num, int num_blocks) {
  if (!check_initialized("load_from_disk")) return false;

  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  this->start_block = start_block_num;
  this->block_count = num_blocks;

  auto buffer = BlockUtils::create_block_buffer();
  char* current_bitmap_ptr = bitmap_data;
  int remaining_bytes = bitmap_size;

  for (int i = 0; i < num_blocks; ++i) {
    if (!disk.read_block(start_block_num + i, buffer.get())) {
      ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to read bitmap block: " + std::to_string(start_block_num + i));
      return false;
    }

    int bytes_to_copy = std::min(BLOCK_SIZE, remaining_bytes);
    if (bytes_to_copy <= 0) break;

    BlockUtils::copy_block_data(current_bitmap_ptr, buffer.get(), bytes_to_copy);
    current_bitmap_ptr += bytes_to_copy;
    remaining_bytes -= bytes_to_copy;
  }

  // 加载后重新计算空闲位数以确保一致性
  recalculate_free_bits();
  return true;
}

/**
 * @brief 将位图数据保存到磁盘。
 * @param disk DiskSimulator对象的引用。
 * @param start_block_num 位图在磁盘上的起始块号。
 * @param num_blocks 位图占用的块数。
 * @return bool 成功保存返回true，否则返回false。
 */
bool BitmapManager::save_to_disk(DiskSimulator& disk, int start_block_num, int num_blocks) {
  if (!check_initialized("save_to_disk")) return false;

  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  this->start_block = start_block_num;
  this->block_count = num_blocks;

  auto buffer = BlockUtils::create_block_buffer();
  const char* current_bitmap_ptr = bitmap_data;
  int remaining_bytes = bitmap_size;

  for (int i = 0; i < num_blocks; ++i) {
    BlockUtils::clear_block(buffer.get());

    int bytes_to_copy = std::min(BLOCK_SIZE, remaining_bytes);
    if (bytes_to_copy > 0) {
      BlockUtils::copy_block_data(buffer.get(), current_bitmap_ptr, bytes_to_copy);
      current_bitmap_ptr += bytes_to_copy;
      remaining_bytes -= bytes_to_copy;
    }

    if (!disk.write_block(start_block_num + i, buffer.get())) {
      ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to write bitmap block: " + std::to_string(start_block_num + i));
      return false;
    }
  }

  return true;
}

/** 
 * @brief 获取总位数。
 * @return int 位图的总容量。
 */
int BitmapManager::get_total_bits() const {
  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  return total_bits;
}

/** 
 * @brief 获取空闲位数（O(1)复杂度）。
 * @return int 当前空闲位的数量。
 */
int BitmapManager::get_free_bits() const {
  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  return free_bits_count;
}

/** 
 * @brief 获取已用位数（O(1)复杂度）。
 * @return int 当前已用位的数量。
 */
int BitmapManager::get_used_bits() const {
  std::lock_guard<std::mutex> lock(bitmap_mutex_);
  return total_bits - free_bits_count;
}

// ==============================================================================
// 私有辅助方法
// ==============================================================================

/**
 * @brief 检查位图是否已成功初始化。
 * @param operation_name 正在执行的操作名称，用于日志记录。
 * @return bool 如果已初始化则返回true。
 */
bool BitmapManager::check_initialized(const std::string& operation_name) const {
    if (bitmap_data && bitmap_size > 0) return true;
    ErrorHandler::log_error(ErrorCode::ERROR_OUT_OF_MEMORY, "Bitmap not initialized, cannot perform '" + operation_name + "'");
    return false;
}

/**
 * @brief 检查位号是否在有效范围内 [0, total_bits - 1]。
 * @param bit_num 要验证的位号。
 * @return bool 如果有效则返回true。
 */
bool BitmapManager::is_valid_bit(int bit_num) const {
  return bit_num >= 0 && bit_num < total_bits;
}

/**
 * @brief 获取指定位在位图数据中的具体位置。
 * @param bit_num 逻辑位号。
 * @param[out] byte_index 包含该位的字节索引。
 * @param[out] bit_offset 该位在字节内的偏移量（0-7）。
 */
void BitmapManager::get_bit_location(int bit_num, int& byte_index, int& bit_offset) const {
    byte_index = bit_num / 8;
    bit_offset = bit_num % 8;
}

/**
 * @brief 设置指定位为1（已分配）。
 * @param bit_num 要设置的位号。
 */
void BitmapManager::set_bit(int bit_num) {
  if (!is_valid_bit(bit_num)) return;

  int byte_index, bit_offset;
  get_bit_location(bit_num, byte_index, bit_offset);
  bitmap_data[byte_index] |= (1 << bit_offset);
}

/**
 * @brief 清除指定位为0（未分配）。
 * @param bit_num 要清除的位号。
 */
void BitmapManager::clear_bit(int bit_num) {
  if (!is_valid_bit(bit_num)) return;

  int byte_index, bit_offset;
  get_bit_location(bit_num, byte_index, bit_offset);
  bitmap_data[byte_index] &= ~(1 << bit_offset);
}

/**
 * @brief 查找第一个值为0的空闲位。
 * @return int 空闲位的索引。如果找不到则返回-1。
 */
int BitmapManager::find_free_bit() const {
  if (free_bits_count == 0 || !bitmap_data) {
    return -1;
  }

  for (int bit = 0; bit < total_bits; ++bit) {
    int byte_index = bit / 8;
    int bit_offset = bit % 8;
    if ((bitmap_data[byte_index] & (1 << bit_offset)) == 0) {
      return bit;
    }
  }

  return -1;
}

/**
 * @brief 遍历整个位图，重新计算空闲位的数量。
 * @note 这是一个耗时操作，仅在从磁盘加载或数据不一致时调用。
 */
void BitmapManager::recalculate_free_bits() {
    if (!bitmap_data || total_bits <= 0) {
        free_bits_count = 0;
        return;
    }

    free_bits_count = 0;
    for (int bit = 0; bit < total_bits; ++bit) {
        int byte_index = bit / 8;
        int bit_offset = bit % 8;
        bool allocated = (bitmap_data[byte_index] & (1 << bit_offset)) != 0;
        if (!allocated) {
            free_bits_count++;
        }
    }
}
