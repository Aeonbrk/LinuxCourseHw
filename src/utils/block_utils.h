#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "common.h"

/**
 * @brief 提供通用块操作的静态工具类。
 * 
 * 此类提供静态方法，用于处理整个代码库中与块相关的操作，
 * 包括块计算、对齐、验证和数据操作。
 */
class BlockUtils {
public:
    /**
     * @brief 根据给定的大小计算所需的块数。
     * @param size 字节大小。
     * @return uint32_t 需要的块数。
     */
    static uint32_t calculate_blocks_needed(size_t size);
    
    /**
     * @brief 将给定的大小对齐到块边界。
     * @param size 字节大小。
     * @return size_t 对齐后的字节大小。
     */
    static size_t align_to_block_size(size_t size);
    
    /**
     * @brief 验证块索引是否在有效范围内。
     * @param block_index 要验证的块索引。
     * @return bool 如果有效则返回true，否则返回false。
     */
    static bool is_valid_block_index(uint32_t block_index);
    
    /**
     * @brief 创建并初始化一个块缓冲区。
     * @return std::unique_ptr<char[]> 指向新分配并清零的块缓冲区的智能指针。
     */
    static std::unique_ptr<char[]> create_block_buffer();
    
    /**
     * @brief 安全地复制块数据并进行边界检查。
     * @param dest 目标缓冲区。
     * @param src 源缓冲区。
     * @param size 要复制的字节数。
     * @return bool 如果复制成功返回true，如果大小超过块大小则返回false。
     */
    static bool copy_block_data(char* dest, const char* src, size_t size);
    
    /**
     * @brief 将块数据清零。
     * @param block_data 指向要清除的块数据的指针。
     */
    static void clear_block(char* block_data);
    
    /**
     * @brief 将指定大小的缓冲区清零。
     * @param buffer 指向要清除的缓冲区的指针。
     * @param size 缓冲区的字节大小。
     */
    static void clear_buffer(char* buffer, size_t size);
};