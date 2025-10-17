// ==============================================================================
// @file   file_operations_utils.cpp
// @brief  通用文件操作工具类的实现
// ==============================================================================

#include "file_operations_utils.h"
#include <cstring>
#include <algorithm>

/**
 * @brief 从数据块读取数据到缓冲区。
 * @param disk DiskSimulator的引用。
 * @param blocks 数据块列表。
 * @param offset 偏移量。
 * @param buffer 输出缓冲区。
 * @param size 要读取的大小。
 * @return bool 读取成功返回true，否则返回false。
 */
bool FileOperationsUtils::read_data_from_blocks(DiskSimulator& disk, 
                                               const std::vector<int>& blocks, 
                                               int offset, 
                                               char* buffer, 
                                               int size) {
    if (blocks.empty()) {
        return false;
    }

    int bytes_read = 0;
    int start_block = offset / BLOCK_SIZE;
    int start_offset = offset % BLOCK_SIZE;

    for (int i = start_block;
         i < static_cast<int>(blocks.size()) && bytes_read < size; i++) {
        char block_buffer[BLOCK_SIZE];
        if (!disk.read_block(blocks[i], block_buffer)) {
            return false;
        }

        int copy_size = std::min(BLOCK_SIZE - start_offset, size - bytes_read);
        memcpy(buffer + bytes_read, block_buffer + start_offset, copy_size);

        bytes_read += copy_size;
        start_offset = 0;  // 后续块从开头开始复制
    }

    return bytes_read == size;
}

/**
 * @brief 将缓冲区数据写入到数据块。
 * @param disk DiskSimulator的引用。
 * @param blocks 数据块列表。
 * @param offset 偏移量。
 * @param buffer 输入缓冲区。
 * @param size 要写入的大小。
 * @return bool 写入成功返回true，否则返回false。
 */
bool FileOperationsUtils::write_data_to_blocks(DiskSimulator& disk, 
                                              const std::vector<int>& blocks, 
                                              int offset, 
                                              const char* buffer, 
                                              int size) {
    if (blocks.empty()) {
        return false;
    }

    int bytes_written = 0;
    int start_block = offset / BLOCK_SIZE;
    int start_offset = offset % BLOCK_SIZE;

    for (int i = start_block;
         i < static_cast<int>(blocks.size()) && bytes_written < size; i++) {
        char block_buffer[BLOCK_SIZE];

        if (start_offset > 0 || size - bytes_written < BLOCK_SIZE) {
            if (!disk.read_block(blocks[i], block_buffer)) {
                return false;
            }
        } else {
            memset(block_buffer, 0, BLOCK_SIZE);
        }

        int copy_size = std::min(BLOCK_SIZE - start_offset, size - bytes_written);
        memcpy(block_buffer + start_offset, buffer + bytes_written, copy_size);

        if (!disk.write_block(blocks[i], block_buffer)) {
            return false;
        }

        bytes_written += copy_size;
        start_offset = 0;  // 后续块从开头开始写入
    }

    return bytes_written == size;
}

/**
 * @brief 初始化一个新的inode结构体。
 * @param[out] inode 要被初始化的inode对象。
 * @param mode 文件类型和权限
 * @param link_count 硬链接计数，默认为1
 */
void FileOperationsUtils::initialize_new_inode(Inode& inode, int mode, int link_count) {
    memset(&inode, 0, sizeof(Inode));
    inode.mode = mode;
    inode.owner_id = 0;
    inode.group_id = 0;
    inode.size = 0;
    inode.access_time = time(nullptr);
    inode.modification_time = time(nullptr);
    inode.creation_time = time(nullptr);
    inode.link_count = link_count;
    memset(inode.direct_blocks, 0, sizeof(inode.direct_blocks));
    inode.indirect_block = -1;
    inode.double_indirect_block = -1;
}