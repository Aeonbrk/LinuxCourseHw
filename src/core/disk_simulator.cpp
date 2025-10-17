// ==============================================================================
// @file   disk_simulator.cpp
// @brief  虚拟磁盘模拟器的实现
// ==============================================================================

#include "disk_simulator.h"
#include <cstring>
#include <iostream>
#include <sys/file.h>
#include <unistd.h>

// ==============================================================================
// 构造与析构
// ==============================================================================

/**
 * @brief 构造函数，初始化磁盘状态。
 */
DiskSimulator::DiskSimulator()
    : disk_file(nullptr),
      disk_size(0),
      total_blocks(0),
      disk_open(false),
      lock_acquired(false) {}

/**
 * @brief 析构函数，确保磁盘文件被正确关闭。
 */
DiskSimulator::~DiskSimulator() {
  close_disk();
}

// ==============================================================================
// 公共接口方法：磁盘生命周期管理
// ==============================================================================

/**
 * @brief 创建一个新的虚拟磁盘文件。
 * @details 使用fseek创建一个稀疏文件，速度远快于逐块写入。
 * @param path 要创建的磁盘文件的路径。
 * @param size_mb 磁盘文件的大小（以MB为单位）。
 * @return bool 操作成功返回true，否则返回false。
 */
bool DiskSimulator::create_disk(const std::string& path, int size_mb) {
  if (disk_open) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_OPEN, "Create failed: A disk file is already open");
    return false;
  }

  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to create disk file: " + path);
    return false;
  }

  disk_size = static_cast<long>(size_mb) * 1024 * 1024;
  if (disk_size <= 0) {
      ErrorHandler::log_error(ERROR_INVALID_ARGUMENT, "Disk size must be a positive number");
      fclose(file);
      return false;
  }

  // 使用fseek创建稀疏文件，比逐块写入快得多
  if (fseek(file, disk_size - 1, SEEK_SET) != 0) {
      ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to extend disk file with fseek");
      fclose(file);
      return false;
  }
  if (fwrite("", 1, 1, file) != 1) {
      ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to write the last byte of the disk file");
      fclose(file);
      return false;
  }

  fclose(file);
  disk_path = path;
  return true;
}

/**
 * @brief 打开一个已存在的磁盘文件。
 * @param path 要打开的磁盘文件的路径。
 * @return bool 操作成功返回true，否则返回false。
 */
bool DiskSimulator::open_disk(const std::string& path) {
  if (disk_open) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_OPEN, "Open failed: A disk file is already open");
    return false;
  }

  disk_file = fopen(path.c_str(), "rb+");
  if (!disk_file) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to open disk file: " + path);
    return false;
  }

  int fd = fileno(disk_file);
  if (fd == -1) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to resolve file descriptor for: " + path);
    fclose(disk_file);
    disk_file = nullptr;
    return false;
  }

  if (flock(fd, LOCK_EX) != 0) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to lock disk file: " + path);
    fclose(disk_file);
    disk_file = nullptr;
    return false;
  }
  lock_acquired = true;

  // 计算文件大小和总块数
  fseek(disk_file, 0, SEEK_END);
  disk_size = ftell(disk_file);
  fseek(disk_file, 0, SEEK_SET);
  total_blocks = disk_size / BLOCK_SIZE;

  disk_path = path;
  disk_open = true;
  return true;
}

/**
 * @brief 关闭当前打开的磁盘文件。
 */
void DiskSimulator::close_disk() {
  if (disk_file) {
    if (lock_acquired) {
      int fd = fileno(disk_file);
      if (fd != -1) {
        flock(fd, LOCK_UN);
      }
      lock_acquired = false;
    }
    fclose(disk_file);
    disk_file = nullptr;
  }
  disk_open = false;
}

/**
 * @brief 格式化磁盘，创建文件系统元数据结构。
 * @return bool 操作成功返回true，否则返回false。
 */
bool DiskSimulator::format_disk() {
  if (!disk_open) {
    ErrorHandler::log_error(ERROR_FILE_NOT_OPEN, "Format failed: Disk not open");
    return false;
  }

  DiskLayout layout = calculate_layout();

  if (!initialize_superblock(layout)) return false;
  if (!initialize_bitmaps(layout)) return false;
  if (!initialize_inode_table(layout)) return false;

  return true;
}

// ==============================================================================
// 公共接口方法：块级I/O操作
// ==============================================================================

/**
 * @brief 从磁盘读取一个数据块。
 * @param block_num 要读取的块号。
 * @param buffer 用于存储读取数据的缓冲区（大小应为BLOCK_SIZE）。
 * @return bool 操作成功返回true，否则返回false。
 */
bool DiskSimulator::read_block(int block_num, char* buffer) {
  if (!is_ready_for_io(block_num)) return false;
  
  {
    std::lock_guard<std::mutex> lock(disk_mutex_);
    if (!seek_to_block(block_num)) return false;

    if (fread(buffer, 1, BLOCK_SIZE, disk_file) != BLOCK_SIZE) {
      ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read block: " + std::to_string(block_num));
      return false;
    }
  }

  return true;
}

/**
 * @brief 向磁盘写入一个数据块。
 * @param block_num 要写入的块号。
 * @param buffer 包含要写入数据的缓冲区（大小应为BLOCK_SIZE）。
 * @return bool 操作成功返回true，否则返回false。
 */
bool DiskSimulator::write_block(int block_num, const char* buffer) {
  if (!is_ready_for_io(block_num)) return false;
  
  {
    std::lock_guard<std::mutex> lock(disk_mutex_);
    if (!seek_to_block(block_num)) return false;

    if (fwrite(buffer, 1, BLOCK_SIZE, disk_file) != BLOCK_SIZE) {
      ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to write block: " + std::to_string(block_num));
      return false;
    }

    fflush(disk_file); // 确保数据立即写入磁盘
  }
  return true;
}

// ==============================================================================
// 公共接口方法：Getters
// ==============================================================================

bool DiskSimulator::is_open() const { return disk_open; }
int DiskSimulator::get_total_blocks() const { return total_blocks; }
long DiskSimulator::get_disk_size() const { return disk_size; }
int DiskSimulator::get_block_size() const { return BLOCK_SIZE; }
std::string DiskSimulator::get_disk_path() const { return disk_path; }

/**
 * @brief 计算并返回磁盘布局。
 * @return DiskLayout 包含文件系统各部分布局信息的结构体。
 */
DiskLayout DiskSimulator::calculate_layout() const {
  DiskLayout layout;
  int inodes_per_block = BLOCK_SIZE / sizeof(Inode);

  layout.superblock_blocks = 1;
  layout.superblock_start = 0;

  // inode数量估算为总块数的10%，向上取整到整块
  int inode_count = (total_blocks / 10 + inodes_per_block - 1) / inodes_per_block * inodes_per_block;
  if (inode_count == 0 && total_blocks > 10) inode_count = inodes_per_block;

  layout.inode_table_blocks = inode_count / inodes_per_block;
  layout.inode_table_start = layout.superblock_start + layout.superblock_blocks;

  layout.inode_bitmap_blocks = (inode_count + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
  layout.inode_bitmap_start = layout.inode_table_start + layout.inode_table_blocks;

  layout.data_bitmap_blocks = (total_blocks + BITS_PER_BLOCK - 1) / BITS_PER_BLOCK;
  layout.data_bitmap_start = layout.inode_bitmap_start + layout.inode_bitmap_blocks;

  layout.data_blocks_start = layout.data_bitmap_start + layout.data_bitmap_blocks;
  layout.data_blocks_count = total_blocks > layout.data_blocks_start ? total_blocks - layout.data_blocks_start : 0;

  return layout;
}

// ==============================================================================
// 私有辅助方法
// ==============================================================================

/**
 * @brief 检查磁盘是否为I/O操作就绪且块号是否有效。
 * @param block_num 要检查的块号。
 * @return bool 如果就绪且有效则返回true。
 */
bool DiskSimulator::is_ready_for_io(int block_num) const {
  if (!disk_open) {
    ErrorHandler::log_error(ERROR_FILE_NOT_OPEN, "I/O operation failed: Disk not open");
    return false;
  }
  if (block_num < 0 || block_num >= total_blocks) {
    ErrorHandler::log_error(ERROR_INVALID_BLOCK, "I/O operation failed: Invalid block number: " + std::to_string(block_num));
    return false;
  }
  return true;
}

/**
 * @brief 将文件指针移动到指定块的起始位置。
 * @param block_num 目标块号。
 * @return bool 操作成功返回true。
 */
bool DiskSimulator::seek_to_block(int block_num) {
  long offset = static_cast<long>(block_num) * BLOCK_SIZE;
  if (fseek(disk_file, offset, SEEK_SET) != 0) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to seek to block: " + std::to_string(block_num));
    return false;
  }
  return true;
}

/**
 * @brief 初始化超级块并写入磁盘。
 * @param layout 磁盘布局信息。
 * @return bool 操作成功返回true。
 */
bool DiskSimulator::initialize_superblock(const DiskLayout& layout) {
  Superblock sb;
  int total_inodes = layout.inode_table_blocks * (BLOCK_SIZE / sizeof(Inode));

  sb.magic_number = MAGIC_NUMBER;
  sb.total_blocks = total_blocks;
  sb.free_blocks = layout.data_blocks_count;
  sb.total_inodes = total_inodes;
  sb.free_inodes = total_inodes; // 初始时全部空闲
  sb.block_size = BLOCK_SIZE;
  sb.inode_table_start = layout.inode_table_start;
  sb.data_blocks_start = layout.data_blocks_start;
  sb.inode_bitmap_start = layout.inode_bitmap_start;
  sb.data_bitmap_start = layout.data_bitmap_start;
  sb.mount_time = time(nullptr);
  sb.write_time = time(nullptr);

  auto buffer = BlockUtils::create_block_buffer();
  BlockUtils::copy_block_data(buffer.get(), reinterpret_cast<const char*>(&sb), sizeof(Superblock));

  return write_block(layout.superblock_start, buffer.get());
}

/**
 * @brief 初始化inode和数据块位图区域（清零）。
 * @param layout 磁盘布局信息。
 * @return bool 操作成功返回true。
 */
bool DiskSimulator::initialize_bitmaps(const DiskLayout& layout) {
  if (!write_zeroed_blocks(layout.inode_bitmap_start, layout.inode_bitmap_blocks)) return false;
  if (!write_zeroed_blocks(layout.data_bitmap_start, layout.data_bitmap_blocks)) return false;
  return true;
}

/**
 * @brief 初始化inode表区域（清零）。
 * @param layout 磁盘布局信息。
 * @return bool 操作成功返回true。
 */
bool DiskSimulator::initialize_inode_table(const DiskLayout& layout) {
  return write_zeroed_blocks(layout.inode_table_start, layout.inode_table_blocks);
}

/**
 * @brief 将指定数量的块清零写入磁盘。
 * @param start_block 起始块号。
 * @param num_blocks 要清零的块数量。
 * @return bool 操作成功返回true。
 */
bool DiskSimulator::write_zeroed_blocks(int start_block, int num_blocks) {
    if (num_blocks <= 0) return true;
    auto buffer = BlockUtils::create_block_buffer(); // 已自动清零
    for (int i = 0; i < num_blocks; ++i) {
        if (!write_block(start_block + i, buffer.get())) {
            ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to zero out block region, start block: " + std::to_string(start_block));
            return false;
        }
    }
    return true;
}
