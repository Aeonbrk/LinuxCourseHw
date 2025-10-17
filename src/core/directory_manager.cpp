// ==============================================================================
// @file   directory_manager.cpp
// @brief  目录管理模块的实现
// ==============================================================================

#include "directory_manager.h"

#include <algorithm>
#include <cstring>
#include <string>

// ==============================================================================
// 构造与析构
// ==============================================================================

/**
 * @brief 构造函数。
 * @param disk DiskSimulator对象的引用。
 * @param inode_manager InodeManager对象的引用。
 * @param path_manager PathManager对象的引用。
 */
DirectoryManager::DirectoryManager(DiskSimulator& disk,
                                   InodeManager& inode_manager,
                                   PathManager& path_manager)
    : disk(disk), inode_manager(inode_manager), path_manager(path_manager) {
}

// ==============================================================================
// 公共接口方法
// ==============================================================================

/**
 * @brief 创建新目录，分配inode并初始化目录结构。
 * @param path 要创建的目录路径。
 * @return bool 创建成功返回true，否则返回false。
 */
bool DirectoryManager::create_directory(const std::string& path) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  if (path_manager.file_exists(path)) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_EXISTS,
                            "Directory already exists: " + path);
    return false;
  }

  std::string parent_path = path_manager.get_parent_path(path);
  std::string basename = path_manager.get_basename(path);

  int parent_inode = path_manager.find_inode(parent_path);
  if (parent_inode == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Parent directory not found: " + parent_path);
    return false;
  }

  // 分配新的inode
  int new_inode;
  if (!inode_manager.allocate_inode(new_inode)) {
    ErrorHandler::log_error(ERROR_NO_FREE_INODES,
                            "Failed to allocate inode for directory: " + path);
    return false;
  }

  // 设置inode属性
  Inode inode;
  FileOperationsUtils::initialize_new_inode(
      inode,
      FILE_TYPE_DIRECTORY | FILE_PERMISSION_READ | FILE_PERMISSION_WRITE |
          FILE_PERMISSION_EXECUTE,
      2);  // link_count = 2 for directories (. and ..)

  // 先写入基本的inode
  if (!inode_manager.write_inode(new_inode, inode)) {
    inode_manager.free_inode(new_inode);
    return false;
  }

  // 分配一个数据块给目录 (这会自动更新inode中的direct_blocks并保存)
  std::vector<int> blocks;
  if (!inode_manager.allocate_data_blocks(new_inode, 1, blocks)) {
    ErrorHandler::log_error(ERROR_NO_FREE_BLOCKS,
                            "Failed to allocate directory data block: " + path);
    inode_manager.free_inode(new_inode);
    return false;
  }

  // 创建目录条目
  std::vector<DirectoryEntry> entries;

  // 添加 "." 条目
  DirectoryEntry dot_entry;
  memset(&dot_entry, 0, sizeof(DirectoryEntry));
  dot_entry.inode_number = new_inode;
  strncpy(dot_entry.name, ".", sizeof(dot_entry.name) - 1);
  dot_entry.name[sizeof(dot_entry.name) - 1] = '\0';
  dot_entry.name_length = 1;
  entries.push_back(dot_entry);

  // 添加 ".." 条目
  DirectoryEntry dotdot_entry;
  memset(&dotdot_entry, 0, sizeof(DirectoryEntry));
  dotdot_entry.inode_number = parent_inode;
  strncpy(dotdot_entry.name, "..", sizeof(dotdot_entry.name) - 1);
  dotdot_entry.name[sizeof(dotdot_entry.name) - 1] = '\0';
  dotdot_entry.name_length = 2;
  entries.push_back(dotdot_entry);

  if (!write_directory(new_inode, entries)) {
    inode_manager.free_inode(new_inode);
    return false;
  }

  // 在父目录中添加条目
  if (!add_directory_entry(parent_inode, basename, new_inode)) {
    inode_manager.free_inode(new_inode);
    return false;
  }

  return true;
}

/**
 * @brief 列出目录内容，返回目录条目列表。
 * @param path 要列出的目录路径。
 * @param entries 用于存储目录条目的向量。
 * @return bool 列出成功返回true，否则返回false。
 */
bool DirectoryManager::list_directory(const std::string& path,
                                      std::vector<DirectoryEntry>& entries) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  int inode_num = path_manager.find_inode(path);
  if (inode_num == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Directory not found: " + path);
    return false;
  }

  return read_directory(inode_num, entries);
}

/**
 * @brief 删除目录，检查是否为空后释放资源。
 * @param path 要删除的目录路径。
 * @return bool 删除成功返回true，否则返回false。
 */
bool DirectoryManager::remove_directory(const std::string& path) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  if (path == "/") {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                            "Cannot remove root directory");
    return false;
  }

  int inode_num = path_manager.find_inode(path);
  if (inode_num == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Directory not found: " + path);
    return false;
  }

  Inode inode;
  if (!load_directory_inode(inode_num, inode)) {
    return false;
  }

  // 检查目录是否为空
  std::vector<DirectoryEntry> entries;
  if (!read_directory(inode_num, entries)) {
    return false;
  }

  if (entries.size() > 2) {  // 超过 . 和 ..
    ErrorHandler::log_error(ERROR_DIRECTORY_NOT_EMPTY,
                            "Directory not empty: " + path);
    return false;
  }

  std::string parent_path = path_manager.get_parent_path(path);
  std::string basename = path_manager.get_basename(path);

  int parent_inode = path_manager.find_inode(parent_path);
  if (parent_inode == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Parent directory not found: " + parent_path);
    return false;
  }

  // 从父目录中移除条目
  if (!remove_directory_entry(parent_inode, basename)) {
    return false;
  }

  // 释放inode和数据块
  return inode_manager.free_inode(inode_num);
}

/**
 * @brief 读取目录内容，返回目录条目列表。
 * @param inode_num 目录的inode号。
 * @param entries 用于存储目录条目的向量。
 * @return bool 读取成功返回true，否则返回false。
 */
bool DirectoryManager::read_directory(int inode_num,
                                      std::vector<DirectoryEntry>& entries) {
  Inode inode;
  if (!load_directory_inode(inode_num, inode)) {
    return false;
  }

  entries.clear();

  if (inode.size == 0) {
    return true;
  }

  // 获取数据块
  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(inode_num, blocks)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to get data blocks for directory inode: " +
                                std::to_string(inode_num));
    return false;
  }

  // 读取目录数据
  char buffer[BLOCK_SIZE];
  for (int block_num : blocks) {
    if (!disk.read_block(block_num, buffer)) {
      ErrorHandler::log_error(
          ERROR_IO_ERROR,
          "Failed to read directory block: " + std::to_string(block_num));
      return false;
    }

    DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(buffer);
    int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < max_entries; i++) {
      if (entry[i].name_length > 0) {
        entries.push_back(entry[i]);
      }
    }
  }

  return true;
}

/**
 * @brief 写入目录内容到磁盘。
 * @param inode_num 目录的inode号。
 * @param entries 要写入的目录条目向量。
 * @return bool 写入成功返回true，否则返回false。
 */
bool DirectoryManager::write_directory(
    int inode_num, const std::vector<DirectoryEntry>& entries) {
  // 读取目录inode
  Inode inode;
  if (!inode_manager.read_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read directory inode: " +
                                                std::to_string(inode_num));
    return false;
  }

  // 使用BlockUtils计算所需的大小和块数
  size_t required_size = entries.size() * sizeof(DirectoryEntry);
  uint32_t required_blocks = BlockUtils::calculate_blocks_needed(required_size);

  // 获取当前块
  std::vector<int> current_blocks;
  if (!inode_manager.get_data_blocks(inode_num, current_blocks)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to get data blocks for directory inode: " +
                                std::to_string(inode_num));
    return false;
  }

  // 如果需要，分配额外的块
  if (current_blocks.size() < required_blocks) {
    uint32_t additional_blocks = required_blocks - current_blocks.size();
    std::vector<int> new_blocks;
    if (!inode_manager.allocate_data_blocks(inode_num, additional_blocks,
                                            new_blocks)) {
      ErrorHandler::log_error(
          ERROR_NO_FREE_BLOCKS,
          "Failed to allocate additional blocks for directory");
      return false;
    }

    // 分配后刷新块列表
    current_blocks.clear();
    if (!inode_manager.get_data_blocks(inode_num, current_blocks)) {
      ErrorHandler::log_error(ERROR_IO_ERROR,
                              "Failed to refresh block list after allocation");
      return false;
    }
  }

  // 将目录条目写入块
  int entries_per_block = BLOCK_SIZE / sizeof(DirectoryEntry);
  int entry_index = 0;

  for (size_t i = 0; i < current_blocks.size(); i++) {
    // 创建并初始化块缓冲区
    auto block_buffer = BlockUtils::create_block_buffer();
    DirectoryEntry* entry_ptr =
        reinterpret_cast<DirectoryEntry*>(block_buffer.get());

    // 用目录条目填充块
    for (int j = 0; j < entries_per_block &&
                    entry_index < static_cast<int>(entries.size());
         j++) {
      entry_ptr[j] = entries[entry_index];
      entry_index++;
    }

    // 写入前验证块索引
    if (!BlockUtils::is_valid_block_index(
            static_cast<uint32_t>(current_blocks[i]))) {
      ErrorHandler::log_error(
          ERROR_INVALID_BLOCK,
          "Invalid block index: " + std::to_string(current_blocks[i]));
      return false;
    }

    // 将块写入磁盘
    if (!disk.write_block(current_blocks[i], block_buffer.get())) {
      ErrorHandler::log_error(ERROR_IO_ERROR,
                              "Failed to write directory block: " +
                                  std::to_string(current_blocks[i]));
      return false;
    }
  }

  // 更新inode的大小和修改时间
  inode.size = required_size;
  inode.modification_time = time(nullptr);

  if (!inode_manager.write_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to update directory inode");
    return false;
  }

  return true;
}

/**
 * @brief 在目录中添加新的条目。
 * @param dir_inode 目录的inode号。
 * @param name 要添加的条目名称。
 * @param inode_num 对应的inode号。
 * @return bool 添加成功返回true，否则返回false。
 */
bool DirectoryManager::add_directory_entry(int dir_inode,
                                           const std::string& name,
                                           int inode_num) {
  std::vector<DirectoryEntry> entries;
  if (!read_directory(dir_inode, entries)) {
    return false;
  }

  // 检查条目是否已存在
  if (find_entry_index(entries, name) != -1) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_EXISTS,
                            "Directory entry already exists: " + name);
    return false;
  }

  // 添加新条目
  DirectoryEntry new_entry;
  memset(&new_entry, 0, sizeof(DirectoryEntry));
  new_entry.inode_number = inode_num;
  int name_len =
      std::min(static_cast<int>(name.length()), MAX_FILENAME_LENGTH - 1);
  strncpy(new_entry.name, name.c_str(), name_len);
  new_entry.name[name_len] = '\0';
  new_entry.name_length = name_len;

  entries.push_back(new_entry);

  return write_directory(dir_inode, entries);
}

/**
 * @brief 从目录中移除条目。
 * @param dir_inode 目录的inode号。
 * @param name 要移除的条目名称。
 * @return bool 移除成功返回true，否则返回false。
 */
bool DirectoryManager::remove_directory_entry(int dir_inode,
                                              const std::string& name) {
  std::vector<DirectoryEntry> entries;
  if (!read_directory(dir_inode, entries)) {
    return false;
  }

  int index = find_entry_index(entries, name);
  if (index == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Directory entry not found: " + name);
    return false;
  }

  entries.erase(entries.begin() + index);
  return write_directory(dir_inode, entries);
}

bool DirectoryManager::load_directory_inode(int inode_num, Inode& inode) {
  if (!inode_manager.read_inode(inode_num, inode)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR, "Failed to read inode: " + std::to_string(inode_num));
    return false;
  }

  if (!(inode.mode & FILE_TYPE_DIRECTORY)) {
    ErrorHandler::log_error(
        ERROR_NOT_A_DIRECTORY,
        "Inode is not a directory: " + std::to_string(inode_num));
    return false;
  }

  return true;
}

int DirectoryManager::find_entry_index(
    const std::vector<DirectoryEntry>& entries, const std::string& name) const {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (strncmp(entries[i].name, name.c_str(), MAX_FILENAME_LENGTH) == 0) {
      return static_cast<int>(i);
    }
  }
  return -1;
}