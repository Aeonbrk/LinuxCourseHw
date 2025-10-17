// ==============================================================================
// @file   inode_manager.cpp
// @brief  Inode管理器的实现
// ==============================================================================

#include "inode_manager.h"
#include "disk_simulator.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <memory> // for std::unique_ptr

// ==============================================================================
// 构造与析构
// ==============================================================================

/**
 * @brief 构造函数，初始化成员变量。
 * @param disk 对磁盘模拟器的引用。
 */
InodeManager::InodeManager(DiskSimulator& disk)
    : disk(disk), inode_bitmap(nullptr), data_bitmap(nullptr), initialized(false) {}

/**
 * @brief 析构函数，释放位图管理器所占用的内存。
 */
InodeManager::~InodeManager() {
    delete inode_bitmap;
    delete data_bitmap;
}

// ==============================================================================
// 公共接口方法
// ==============================================================================

/**
 * @brief 初始化Inode管理器。
 * @param layout 磁盘布局信息。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::initialize(const DiskLayout& layout) {
    this->layout = layout;
    
    int total_inodes = layout.inode_table_blocks * (BLOCK_SIZE / sizeof(Inode));
    int total_data_blocks = layout.data_blocks_count;
    
    // 使用智能指针或手动管理清理旧资源
    delete inode_bitmap;
    inode_bitmap = nullptr;
    delete data_bitmap;
    data_bitmap = nullptr;

    try {
        inode_bitmap = new BitmapManager(total_inodes);
        data_bitmap = new BitmapManager(total_data_blocks);
    } catch (const std::bad_alloc& e) {
        ErrorHandler::log_error(ErrorCode::ERROR_OUT_OF_MEMORY, "Failed to create bitmap managers");
        return false;
    }
    
    if (!load_bitmaps()) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to load bitmaps from disk");
        return false;
    }
    
    initialized = true;
    return true;
}

/**
 * @brief 分配一个空闲的inode。
 * @param[out] inode_num 分配到的inode号。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::allocate_inode(int& inode_num) {
    if (!check_initialized("allocate_inode")) return false;
    
    if (!inode_bitmap->allocate_bit(inode_num)) {
        ErrorHandler::log_error(ErrorCode::ERROR_NO_FREE_INODES, "No free inodes available");
        return false;
    }
    
    // 初始化新分配的inode
    Inode new_inode;
    initialize_new_inode(new_inode);
    
    if (!write_inode(inode_num, new_inode)) {
        ErrorHandler::log_error(ErrorCode::ERROR_INVALID_INODE, "Failed to write newly allocated inode: " + std::to_string(inode_num));
        inode_bitmap->free_bit(inode_num); // 回滚分配
        return false;
    }
    
    if (!save_inode_bitmap()) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to save inode bitmap to disk");
        // 尝试回滚内存中的分配状态
        inode_bitmap->free_bit(inode_num); 
        // 写入操作可能已部分成功，这是一个临界状态
        return false;
    }
    
    return true;
}

/**
 * @brief 释放一个inode及其关联的所有数据块。
 * @param inode_num 要释放的inode号。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::free_inode(int inode_num) {
    if (!check_initialized("free_inode")) return false;
    
    if (!is_inode_allocated(inode_num)) {
        ErrorHandler::log_error(ErrorCode::ERROR_INVALID_ARGUMENT, "Inode " + std::to_string(inode_num) + " is not allocated");
        return false;
    }
    
    if (!free_data_blocks(inode_num)) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to free data blocks for inode " + std::to_string(inode_num));
        // 保持inode为已分配状态，以便后续诊断
        return false; 
    }
    
    inode_bitmap->free_bit(inode_num);
    
    if (!save_inode_bitmap()) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to save bitmap after freeing inode " + std::to_string(inode_num));
        // 这是一个严重问题，但inode已在内存中释放
        return false;
    }
    
    return true;
}

/**
 * @brief 从磁盘读取指定的inode信息。
 * @param inode_num 要读取的inode号。
 * @param[out] inode 读取到的inode对象。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::read_inode(int inode_num, Inode& inode) {
    if (!check_initialized("read_inode")) return false;

    int block_num, offset_in_block;
    if (!get_inode_position(inode_num, block_num, offset_in_block)) {
        return false; // 错误已在get_inode_position中记录
    }

    auto buffer = BlockUtils::create_block_buffer();
    if (!disk.read_block(block_num, buffer.get())) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to read block for inode " + std::to_string(inode_num));
        return false;
    }
    
    memcpy(&inode, buffer.get() + offset_in_block, sizeof(Inode));
    return true;
}

/**
 * @brief 将inode信息写入磁盘。
 * @param inode_num 要写入的inode号。
 * @param inode 要写入的inode对象。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::write_inode(int inode_num, const Inode& inode) {
    if (!check_initialized("write_inode")) return false;

    int block_num, offset_in_block;
    if (!get_inode_position(inode_num, block_num, offset_in_block)) {
        return false;
    }

    auto buffer = BlockUtils::create_block_buffer();
    // Read-Modify-Write: 必须先读出整个块，再修改，以免破坏块内其他inode
    if (!disk.read_block(block_num, buffer.get())) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to read block for writing inode " + std::to_string(inode_num));
        return false;
    }
    
    memcpy(buffer.get() + offset_in_block, &inode, sizeof(Inode));
    
    if (!disk.write_block(block_num, buffer.get())) {
        ErrorHandler::log_error(ErrorCode::ERROR_IO_ERROR, "Failed to write block for inode " + std::to_string(inode_num));
        return false;
    }
    
    return true;
}

/**
 * @brief 为指定的inode分配数据块。
 * @param inode_num inode号。
 * @param block_count 需要分配的数据块数量。
 * @param[out] new_block_nums 分配到的新数据块的编号列表。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::allocate_data_blocks(int inode_num, int block_count, std::vector<int>& new_block_nums) {
    if (!check_initialized("allocate_data_blocks") || block_count <= 0) return false;

    new_block_nums.clear();
    std::vector<uint32_t> allocated_indices;
    if (!allocate_multiple_blocks(block_count, allocated_indices)) {
        ErrorHandler::log_error(ErrorCode::ERROR_NO_FREE_BLOCKS, "Failed to allocate " + std::to_string(block_count) + " data blocks");
        return false;
    }

    if (!update_inode_block_pointers(inode_num, allocated_indices)) {
        ErrorHandler::log_error(ErrorCode::ERROR_INVALID_INODE, "Failed to update block pointers for inode " + std::to_string(inode_num));
        // 回滚已分配的数据块
        for (uint32_t block_index : allocated_indices) {
            data_bitmap->free_bit(block_index - layout.data_blocks_start);
        }
        return false;
    }

    for (uint32_t block_index : allocated_indices) {
        new_block_nums.push_back(static_cast<int>(block_index));
    }

    return save_data_bitmap();
}

/**
 * @brief 释放指定inode的所有数据块。
 * @param inode_num inode号。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::free_data_blocks(int inode_num) {
    if (!check_initialized("free_data_blocks")) return false;

    Inode inode;
    if (!read_inode(inode_num, inode)) return false;

    // 释放所有数据块
    free_all_data_blocks_for_inode(inode);

    // 更新inode状态并写回
    inode.size = 0;
    if (!write_inode(inode_num, inode)) return false;

    return save_data_bitmap();
}

/**
 * @brief 获取指定inode的所有数据块列表。
 * @param inode_num inode号。
 * @param[out] block_nums 获取到的数据块编号列表。
 * @return bool 成功返回true，失败返回false。
 */
bool InodeManager::get_data_blocks(int inode_num, std::vector<int>& block_nums) {
    if (!check_initialized("get_data_blocks")) return false;

    Inode inode;
    if (!read_inode(inode_num, inode)) return false;

    block_nums.clear();
    // 收集直接块
    for (int block_ptr : inode.direct_blocks) {
        if (block_ptr != 0) block_nums.push_back(block_ptr);
    }

    // 收集间接块
    if (inode.indirect_block != -1) {
        std::vector<int> indirect_blocks;
        if (read_indirect_block(inode.indirect_block, indirect_blocks)) {
            block_nums.insert(block_nums.end(), indirect_blocks.begin(), indirect_blocks.end());
        } else {
            return false;
        }
    }
    
    // 处理二级间接块
    if (inode.double_indirect_block != -1) {
        std::vector<int> double_indirect_blocks;
        if (read_indirect_block(inode.double_indirect_block, double_indirect_blocks)) {
            for (int indirect_block_num : double_indirect_blocks) {
                std::vector<int> indirect_blocks;
                if (read_indirect_block(indirect_block_num, indirect_blocks)) {
                    block_nums.insert(block_nums.end(), indirect_blocks.begin(), indirect_blocks.end());
                } else {
                    return false; // 读取二级间接块指向的间接块失败
                }
            }
        } else {
            return false; // 读取二级间接块失败
        }
    }
    
    return true;
}

/** 
 * @brief 检查inode是否已分配。
 * @param inode_num inode号。
 * @return bool 如果已分配则返回true。
 */
bool InodeManager::is_inode_allocated(int inode_num) const {
    if (!initialized || !inode_bitmap) return false;
    return inode_bitmap->is_allocated(inode_num);
}

/** 
 * @brief 获取总inode数量。
 * @return int 总inode数。
 */
int InodeManager::get_total_inodes() const {
    return (initialized && inode_bitmap) ? inode_bitmap->get_total_bits() : 0;
}

/** 
 * @brief 获取空闲inode数量。
 * @return int 空闲inode数。
 */
int InodeManager::get_free_inodes() const {
    return (initialized && inode_bitmap) ? inode_bitmap->get_free_bits() : 0;
}

/** 
 * @brief 获取空闲数据块数量。
 * @return int 空闲数据块数。
 */
int InodeManager::get_free_data_blocks() const {
    return (initialized && data_bitmap) ? data_bitmap->get_free_bits() : 0;
}

/** 
 * @brief 重新从磁盘加载位图。
 * @return bool 成功返回true。
 */
bool InodeManager::reload_bitmap() {
    if (!check_initialized("reload_bitmap")) return false;
    return load_bitmaps();
}

// ==============================================================================
// 私有辅助方法
// ==============================================================================

/**
 * @brief 检查管理器是否已初始化。
 * @param operation_name 执行操作的名称，用于日志记录。
 * @return bool 如果已初始化则返回true。
 */
bool InodeManager::check_initialized(const std::string& operation_name) const {
    if (initialized) return true;
    ErrorHandler::log_error(ErrorCode::ERROR_INVALID_ARGUMENT, "InodeManager not initialized, cannot perform '" + operation_name + "'");
    return false;
}

/**
 * @brief 从磁盘加载inode和数据位图。
 * @return bool 全部加载成功返回true。
 */
bool InodeManager::load_bitmaps() {
    return inode_bitmap->load_from_disk(disk, layout.inode_bitmap_start, layout.inode_bitmap_blocks) &&
           data_bitmap->load_from_disk(disk, layout.data_bitmap_start, layout.data_bitmap_blocks);
}

/**
 * @brief 将inode位图保存到磁盘。
 * @return bool 保存成功返回true。
 */
bool InodeManager::save_inode_bitmap() {
    return inode_bitmap->save_to_disk(disk, layout.inode_bitmap_start, layout.inode_bitmap_blocks);
}

/**
 * @brief 将数据位图保存到磁盘。
 * @return bool 保存成功返回true。
 */
bool InodeManager::save_data_bitmap() {
    return data_bitmap->save_to_disk(disk, layout.data_bitmap_start, layout.data_bitmap_blocks);
}

/**
 * @brief 计算并获取指定inode在磁盘上的位置。
 * @param inode_num inode号。
 * @param[out] block_num inode所在的数据块号。
 * @param[out] offset_in_block inode在块内的偏移量。
 * @return bool 如果inode号有效则返回true。
 */
bool InodeManager::get_inode_position(int inode_num, int& block_num, int& offset_in_block) const {
    if (inode_num < 0 || inode_num >= get_total_inodes()) {
        ErrorHandler::log_error(ErrorCode::ERROR_INVALID_INODE, "Invalid inode number: " + std::to_string(inode_num));
        block_num = -1;
        offset_in_block = -1;
        return false;
    }
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    block_num = layout.inode_table_start + (inode_num / inodes_per_block);
    offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);
    return true;
}

/**
 * @brief 初始化一个新的inode结构体。
 * @param[out] inode 要被初始化的inode对象。
 */
void InodeManager::initialize_new_inode(Inode& inode) {
    memset(&inode, 0, sizeof(Inode));
    time_t now = time(nullptr);
    inode.creation_time = now;
    inode.modification_time = now;
    inode.access_time = now;
    inode.link_count = 1; // 初始硬链接数为1
    inode.indirect_block = -1;
    inode.double_indirect_block = -1;
}

/**
 * @brief 释放一个inode指向的所有数据块。
 * @param inode 需要释放数据块的inode对象。
 */
void InodeManager::free_all_data_blocks_for_inode(Inode& inode) {
    // 释放直接块
    for (int& block_ptr : inode.direct_blocks) {
        if (block_ptr != 0) {
            data_bitmap->free_bit(block_ptr - layout.data_blocks_start);
            block_ptr = 0;
        }
    }

    // 释放间接块
    if (inode.indirect_block != -1) {
        std::vector<int> indirect_blocks;
        if (read_indirect_block(inode.indirect_block, indirect_blocks)) {
            for (int block : indirect_blocks) {
                data_bitmap->free_bit(block - layout.data_blocks_start);
            }
        }
        free_indirect_block(inode.indirect_block);
        inode.indirect_block = -1;
    }

    // 释放二级间接块
    if (inode.double_indirect_block != -1) {
        // 注意：当前实现简化，仅释放其自身，未处理其指向的间接块列表
        free_indirect_block(inode.double_indirect_block);
        inode.double_indirect_block = -1;
    }
}

/**
 * @brief 从间接块中读取其指向的数据块列表。
 * @param block_num 间接块的编号。
 * @param[out] data_blocks 读取到的数据块编号列表。
 * @return bool 成功返回true。
 */
bool InodeManager::read_indirect_block(int block_num, std::vector<int>& data_blocks) {
    auto buffer = BlockUtils::create_block_buffer();
    if (!disk.read_block(block_num, buffer.get())) return false;

    data_blocks.clear();
    int* blocks = reinterpret_cast<int*>(buffer.get());
    int count = BLOCK_SIZE / sizeof(int);
    for (int i = 0; i < count && blocks[i] != 0; ++i) {
        data_blocks.push_back(blocks[i]);
    }
    return true;
}

/**
 * @brief 将数据块列表写入间接块。
 * @param block_num 间接块的编号。
 * @param data_blocks 要写入的数据块编号列表。
 * @return bool 成功返回true。
 */
bool InodeManager::write_indirect_block(int block_num, const std::vector<int>& data_blocks) {
    auto buffer = BlockUtils::create_block_buffer();
    int* blocks = reinterpret_cast<int*>(buffer.get());
    size_t max_blocks = BLOCK_SIZE / sizeof(int);
    
    // 确保缓冲区清零
    memset(buffer.get(), 0, BLOCK_SIZE);

    for (size_t i = 0; i < data_blocks.size() && i < max_blocks; ++i) {
        blocks[i] = data_blocks[i];
    }
    return disk.write_block(block_num, buffer.get());
}

/**
 * @brief 分配一个用于间接块的新的数据块。
 * @param[out] block_num 分配到的数据块编号。
 * @return bool 成功返回true。
 */
bool InodeManager::allocate_indirect_block(int& block_num) {
    int bit_num;
    if (!data_bitmap->allocate_bit(bit_num)) return false;
    block_num = layout.data_blocks_start + bit_num;

    // 将新分配的块清零
    auto buffer = BlockUtils::create_block_buffer();
    return disk.write_block(block_num, buffer.get());
}

/**
 * @brief 释放一个间接块。
 * @param block_num 要释放的间接块编号。
 * @return bool 成功返回true。
 */
bool InodeManager::free_indirect_block(int block_num) {
    if (block_num == -1) return true; // 如果块号无效，则认为操作成功
    return data_bitmap->free_bit(block_num - layout.data_blocks_start);
}

/**
 * @brief 分配一个单独的数据块。
 * @param[out] block_index 分配到的数据块的绝对索引。
 * @return bool 成功返回true。
 */
bool InodeManager::allocate_single_block(uint32_t& block_index) {
    int bit_num;
    if (!data_bitmap->allocate_bit(bit_num)) return false;
    block_index = layout.data_blocks_start + bit_num;
    return true;
}

/**
 * @brief 分配多个数据块。
 * @param count 要分配的数量。
 * @param[out] block_indices 分配到的数据块索引列表。
 * @return bool 成功返回true。
 */
bool InodeManager::allocate_multiple_blocks(size_t count, std::vector<uint32_t>& block_indices) {
    block_indices.clear();
    block_indices.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        uint32_t block_index;
        if (!allocate_single_block(block_index)) {
            // 如果分配失败，回滚本次已分配的所有块
            for (uint32_t allocated_index : block_indices) {
                data_bitmap->free_bit(allocated_index - layout.data_blocks_start);
            }
            block_indices.clear();
            return false;
        }
        block_indices.push_back(block_index);
    }
    return true;
}

/**
 * @brief 将新分配的数据块指针更新到inode中。
 * @param inode_id inode号。
 * @param block_indices 新分配的数据块索引列表。
 * @return bool 成功返回true。
 */
bool InodeManager::update_inode_block_pointers(uint32_t inode_id, const std::vector<uint32_t>& block_indices) {
    Inode inode;
    if (!read_inode(inode_id, inode)) return false;

    std::vector<int> all_blocks;
    if (!get_data_blocks(inode_id, all_blocks)) return false;

    all_blocks.insert(all_blocks.end(), block_indices.begin(), block_indices.end());

    // 清空旧的指针
    memset(inode.direct_blocks, 0, sizeof(inode.direct_blocks));
    if (inode.indirect_block != -1) {
        // 注意：这里简化了处理，实际应先释放旧的间接块
        free_indirect_block(inode.indirect_block);
        inode.indirect_block = -1;
    }

    // 重新填充指针
    for (size_t i = 0; i < all_blocks.size(); ++i) {
        if (i < DIRECT_BLOCKS_COUNT) {
            inode.direct_blocks[i] = all_blocks[i];
        } else {
            size_t blocks_in_indirect = BLOCK_SIZE / sizeof(int);
            size_t relative_index = i - DIRECT_BLOCKS_COUNT;

            if (relative_index < blocks_in_indirect) {
                // --- 单间接块逻辑 ---
                if (relative_index == 0) {
                    if (inode.indirect_block == -1 && !allocate_indirect_block(inode.indirect_block)) {
                        return false;
                    }
                }
                std::vector<int> indirect_blocks;
                if (inode.indirect_block != -1) {
                    read_indirect_block(inode.indirect_block, indirect_blocks);
                }
                indirect_blocks.push_back(all_blocks[i]);
                if (!write_indirect_block(inode.indirect_block, indirect_blocks)) {
                    return false;
                }
            } else {
                // --- 双间接块逻辑 ---
                if (inode.double_indirect_block == -1) {
                    if (!allocate_indirect_block(inode.double_indirect_block)) {
                        ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to allocate double indirect block");
                        return false;
                    }
                }

                size_t double_relative_index = relative_index - blocks_in_indirect;
                size_t d_indirect_idx = double_relative_index / blocks_in_indirect;

                if (d_indirect_idx >= blocks_in_indirect) {
                    ErrorHandler::log_error(ERROR_DISK_FULL, "File size exceeds double indirect block limit");
                    return false;
                }

                std::vector<int> double_indirect_block_pointers;
                read_indirect_block(inode.double_indirect_block, double_indirect_block_pointers);

                int target_indirect_block_num = 0;
                if (d_indirect_idx < double_indirect_block_pointers.size()) {
                    target_indirect_block_num = double_indirect_block_pointers[d_indirect_idx];
                }

                if (target_indirect_block_num == 0) {
                    if (!allocate_indirect_block(target_indirect_block_num)) {
                        ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to allocate indirect block within double indirect block");
                        return false;
                    }
                    if (d_indirect_idx >= double_indirect_block_pointers.size()) {
                        double_indirect_block_pointers.resize(d_indirect_idx + 1, 0);
                    }
                    double_indirect_block_pointers[d_indirect_idx] = target_indirect_block_num;
                    if (!write_indirect_block(inode.double_indirect_block, double_indirect_block_pointers)) {
                        return false;
                    }
                }

                std::vector<int> indirect_blocks;
                read_indirect_block(target_indirect_block_num, indirect_blocks);
                indirect_blocks.push_back(all_blocks[i]);
                if (!write_indirect_block(target_indirect_block_num, indirect_blocks)) {
                    return false;
                }
            }
        }
    }

    inode.modification_time = time(nullptr);
    return write_inode(inode_id, inode);
}
