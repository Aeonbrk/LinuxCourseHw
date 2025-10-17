// ==============================================================================
// @file   block_utils.cpp
// @brief  通用块操作的静态工具类的实现
// ==============================================================================

#include "block_utils.h"
#include <cstring>
#include <memory>
#include <cstdint>

/**
 * @brief 根据给定的大小计算所需的块数。
 * 
 * @param size 字节大小。
 * @return uint32_t 需要的块数，向上取整。
 */
uint32_t BlockUtils::calculate_blocks_needed(size_t size) {
    return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

/**
 * @brief 将给定的大小对齐到块边界。
 * 
 * @param size 字节大小。
 * @return size_t 对齐后的字节大小。
 */
size_t BlockUtils::align_to_block_size(size_t size) {
    return ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
}

/**
 * @brief 验证块索引是否在有效范围内。
 * 
 * @param block_index 要验证的块索引。
 * @return bool 如果有效则返回true，否则返回false。
 */
bool BlockUtils::is_valid_block_index(uint32_t block_index) {
    uint32_t total_blocks = DISK_SIZE / BLOCK_SIZE;
    return block_index < total_blocks;
}

/**
 * @brief 创建并初始化一个块缓冲区。
 * 
 * @return std::unique_ptr<char[]> 指向新分配并清零的块缓冲区的智能指针。
 */
std::unique_ptr<char[]> BlockUtils::create_block_buffer() {
    std::unique_ptr<char[]> buffer(new char[BLOCK_SIZE]);
    memset(buffer.get(), 0, BLOCK_SIZE);
    return buffer;
}

/**
 * @brief 安全地复制块数据并进行边界检查。
 * 
 * @param dest 目标缓冲区。
 * @param src 源缓冲区。
 * @param size 要复制的字节数。
 * @return bool 如果复制成功返回true，如果大小超过块大小则返回false。
 */
bool BlockUtils::copy_block_data(char* dest, const char* src, size_t size) {
    if (size > BLOCK_SIZE || !dest || !src) {
        return false;
    }
    memcpy(dest, src, size);
    return true;
}

/**
 * @brief 将块数据清零。
 * 
 * @param block_data 指向要清除的块数据的指针。
 */
void BlockUtils::clear_block(char* block_data) {
    if (block_data) {
        memset(block_data, 0, BLOCK_SIZE);
    }
}

/**
 * @brief 将指定大小的缓冲区清零。
 * 
 * @param buffer 指向要清除的缓冲区的指针。
 * @param size 缓冲区的字节大小。
 */
void BlockUtils::clear_buffer(char* buffer, size_t size) {
    if (buffer) {
        memset(buffer, 0, size);
    }
}