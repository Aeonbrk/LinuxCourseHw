// ==============================================================================
// @file   filesystem.cpp
// @brief  文件系统高级API的实现
// ==============================================================================

#include "filesystem.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>

// 文件系统构造函数，初始化成员变量
FileSystem::FileSystem()
    : inode_manager(disk),
      mounted(false),
      next_fd(3),
      path_manager(disk, inode_manager),
      directory_manager(disk, inode_manager, path_manager),
      file_manager(disk, inode_manager, path_manager, directory_manager,
                   file_descriptors, next_fd) {
}

// 文件系统析构函数，确保在销毁时卸载文件系统
FileSystem::~FileSystem() {
  if (mounted) {
    unmount();
  }
}

// 挂载文件系统，从磁盘文件加载超级块和位图
bool FileSystem::mount(const std::string& disk_path) {
  if (mounted) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                            "File system already mounted");
    return false;
  }

  if (!disk.open_disk(disk_path)) {
    return false;
  }

  if (!initialize_after_open()) {
    disk.close_disk();
    return false;
  }

  mounted = true;
  return true;
}

// 卸载文件系统，关闭所有打开的文件并关闭磁盘
bool FileSystem::unmount() {
  if (!ensure_mounted("unmount")) {
    return false;
  }

  close_all_files();
  disk.close_disk();
  mounted = false;
  return true;
}

// 格式化已挂载的文件系统，重新加载位图
bool FileSystem::format() {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("format")) {
    return false;
  }

  if (!disk.format_disk()) {
    return false;
  }

  if (!load_superblock()) {
    return false;
  }

  if (!inode_manager.reload_bitmap()) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to reload bitmaps after format");
    return false;
  }

  if (!ensure_root_directory()) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to initialize root directory after format");
    return false;
  }

  return true;
}

// 检查文件系统是否已挂载
bool FileSystem::is_mounted() const {
  auto guard = acquire_shared_lock();
  return mounted;
}

// 创建新文件，分配inode并在父目录中添加条目
int FileSystem::create_file(const std::string& path, int mode) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("create_file")) {
    return -1;
  }

  std::string normalized_path = PathUtils::normalize_path(path);

  // 检查文件是否已存在
  if (path_manager.file_exists(normalized_path)) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_EXISTS,
                            "File already exists: " + normalized_path);
    return -1;
  }

  // 使用辅助方法验证和解析路径
  std::string filename, directory;
  if (!validate_and_parse_path(normalized_path, filename, directory)) {
    return -1;
  }

  // 查找父目录inode
  int parent_inode = path_manager.find_inode(directory);
  if (parent_inode == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Parent directory not found: " + directory);
    return -1;
  }

  // 使用辅助方法分配和初始化文件inode
  int new_inode = allocate_file_inode(filename);
  if (new_inode == -1) {
    return -1;  // 错误已由辅助方法记录
  }

  // 使用提供的权限更新inode模式
  Inode inode;
  if (!inode_manager.read_inode(new_inode, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read allocated inode");
    inode_manager.free_inode(new_inode);
    return -1;
  }

  inode.mode = FILE_TYPE_REGULAR | mode;
  if (!inode_manager.write_inode(new_inode, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to update inode mode");
    inode_manager.free_inode(new_inode);
    return -1;
  }

  // 将目录条目添加到父目录
  if (!add_directory_entry(parent_inode, filename, new_inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to add directory entry for: " + filename);
    inode_manager.free_inode(new_inode);
    return -1;
  }

  return new_inode;
}

// 删除文件，从父目录中移除条目并释放inode和数据块
bool FileSystem::delete_file(const std::string& path) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("delete_file")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);

  int inode_num = path_manager.find_inode(normalized_path);
  if (inode_num == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "File not found: " + normalized_path);
    return false;
  }

  Inode inode;
  if (!inode_manager.read_inode(inode_num, inode)) {
    return false;
  }

  // 检查是否是目录
  if (inode.mode & FILE_TYPE_DIRECTORY) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                            "Use remove_directory for directories");
    return false;
  }

  std::string basename;
  std::string parent_path;
  if (!validate_and_parse_path(normalized_path, basename, parent_path)) {
    return false;
  }

  int parent_inode = path_manager.find_inode(parent_path);
  if (parent_inode == -1) {
    return false;
  }

  // 从父目录中移除条目
  if (!remove_directory_entry(parent_inode, basename)) {
    return false;
  }

  // 释放inode和数据块
  return inode_manager.free_inode(inode_num);
}

// 检查文件是否存在
bool FileSystem::file_exists(const std::string& path) {
  auto guard = acquire_shared_lock();
  if (!ensure_mounted("file_exists")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return path_manager.file_exists(normalized_path);
}

// 打开文件，分配文件描述符
int FileSystem::open_file(const std::string& path, int mode) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("open_file")) {
    return -1;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return file_manager.open_file(normalized_path, mode);
}

// 关闭文件，释放文件描述符并更新修改时间
bool FileSystem::close_file(int fd) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("close_file")) {
    return false;
  }

  return close_file_internal(fd);
}

bool FileSystem::close_file_internal(int fd) {
  return file_manager.close_file(fd);
}

// 从文件中读取数据
int FileSystem::read_file(int fd, char* buffer, int size) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("read_file")) {
    return -1;
  }

  return file_manager.read_file(fd, buffer, size);
}

// 向文件中写入数据
int FileSystem::write_file(int fd, const char* buffer, int size) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("write_file")) {
    return -1;
  }

  return file_manager.write_file(fd, buffer, size);
}

// 设置文件读写位置
bool FileSystem::seek_file(int fd, int position) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("seek_file")) {
    return false;
  }

  return file_manager.seek_file(fd, position);
}

// 创建新目录，分配inode并初始化目录结构
bool FileSystem::create_directory(const std::string& path) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("create_directory")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return directory_manager.create_directory(normalized_path);
}

// 列出目录内容，返回目录条目列表
bool FileSystem::list_directory(const std::string& path,
                                std::vector<DirectoryEntry>& entries) {
  auto guard = acquire_shared_lock();
  if (!ensure_mounted("list_directory")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return directory_manager.list_directory(normalized_path, entries);
}

// 删除目录，检查是否为空后释放资源
bool FileSystem::remove_directory(const std::string& path) {
  auto guard = acquire_unique_lock();
  if (!ensure_mounted("remove_directory")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return directory_manager.remove_directory(normalized_path);
}

// 获取磁盘信息，包括大小、块数、inode数等
bool FileSystem::get_disk_info(std::string& info) {
  auto guard = acquire_shared_lock();
  if (!ensure_mounted("get_disk_info")) {
    return false;
  }

  std::ostringstream oss;
  oss << "Disk Information:" << std::endl;
  oss << "  Disk Size: " << disk.get_disk_size() / (1024 * 1024) << " MB"
      << std::endl;
  oss << "  Block Size: " << disk.get_block_size() << " bytes" << std::endl;
  oss << "  Total Blocks: " << disk.get_total_blocks() << std::endl;
  oss << "  Free Blocks: " << inode_manager.get_free_data_blocks() << std::endl;
  oss << "  Total Inodes: " << inode_manager.get_total_inodes() << std::endl;
  oss << "  Free Inodes: " << inode_manager.get_free_inodes() << std::endl;
  oss << "  Mount Time: " << ctime(&superblock.mount_time);
  oss << "  Write Time: " << ctime(&superblock.write_time);

  info = oss.str();
  return true;
}

bool FileSystem::is_directory(const std::string& path) {
  auto guard = acquire_shared_lock();
  if (!ensure_mounted("is_directory")) {
    return false;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  int inode_num = path_manager.find_inode(normalized_path);
  if (inode_num == -1) {
    return false;
  }

  Inode inode;
  if (!inode_manager.read_inode(inode_num, inode)) {
    return false;
  }

  return (inode.mode & FILE_TYPE_DIRECTORY) != 0;
}

// 获取路径的父目录路径
std::string FileSystem::get_parent_path(const std::string& path) {
  return path_manager.get_parent_path(path);
}

// 获取路径的基本名称（文件名或目录名）
std::string FileSystem::get_basename(const std::string& path) {
  return path_manager.get_basename(path);
}

// 解析路径字符串为路径组件列表
bool FileSystem::parse_path(const std::string& path,
                            std::vector<std::string>& components) {
  // 这将由路径管理器的parse_path方法处理
  // 由于我们已经模块化，我们可以直接公开此功能
  // 或使用PathManager中的现有功能
  return path_manager.parse_path(path, components);
}

// 根据路径查找对应的inode号
int FileSystem::find_inode(const std::string& path) {
  auto guard = acquire_shared_lock();
  if (!ensure_mounted("find_inode")) {
    return -1;
  }

  std::string normalized_path = PathUtils::normalize_path(path);
  return path_manager.find_inode(normalized_path);
}

// 在指定目录中查找文件或子目录的inode号
int FileSystem::find_inode_in_directory(int parent_inode,
                                        const std::string& name) {
  // 此功能在path_manager内部使用
  // 我们可以创建一个临时的直接实现或使用目录
  // 管理器
  std::vector<DirectoryEntry> entries;
  if (!directory_manager.read_directory(parent_inode, entries)) {
    return -1;
  }

  for (const DirectoryEntry& entry : entries) {
    if (strcmp(entry.name, name.c_str()) == 0) {
      return entry.inode_number;
    }
  }

  return -1;
}

// 在目录中添加新的条目
bool FileSystem::add_directory_entry(int dir_inode, const std::string& name,
                                     int inode_num) {
  return directory_manager.add_directory_entry(dir_inode, name, inode_num);
}

// 从目录中移除条目
bool FileSystem::remove_directory_entry(int dir_inode,
                                        const std::string& name) {
  return directory_manager.remove_directory_entry(dir_inode, name);
}

// 读取目录内容，返回目录条目列表
bool FileSystem::read_directory(int inode_num,
                                std::vector<DirectoryEntry>& entries) {
  Inode inode;
  if (!inode_manager.read_inode(inode_num, inode)) {
    return false;
  }

  if (!(inode.mode & FILE_TYPE_DIRECTORY)) {
    return false;
  }

  entries.clear();

  if (inode.size == 0) {
    return true;
  }

  // 获取数据块
  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(inode_num, blocks)) {
    return false;
  }

  // 读取目录数据
  char buffer[BLOCK_SIZE];
  for (int block_num : blocks) {
    if (!disk.read_block(block_num, buffer)) {
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

// 写入目录内容到磁盘
bool FileSystem::write_directory(int inode_num,
                                 const std::vector<DirectoryEntry>& entries) {
  // 读取目录inode
  Inode inode;
  if (!inode_manager.read_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read directory inode: " +
                                                std::to_string(inode_num));
    return false;
  }

  // 使用BlockUtils计算所需的大小和块
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

  // 使用新大小和修改时间更新inode
  inode.size = required_size;
  inode.modification_time = time(nullptr);

  if (!inode_manager.write_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to update directory inode");
    return false;
  }

  return true;
}

// 分配文件描述符
bool FileSystem::allocate_file_descriptor(int inode_num, int mode, int& fd) {
  if (!ensure_mounted("allocate_file_descriptor")) {
    return false;
  }

  return file_manager.allocate_file_descriptor(inode_num, mode, fd);
}

// 获取文件描述符信息
bool FileSystem::get_file_descriptor(int fd, FileDescriptor& desc) {
  if (!ensure_mounted("get_file_descriptor")) {
    return false;
  }

  return file_manager.get_file_descriptor(fd, desc);
}

// 更新文件访问时间
void FileSystem::update_file_access_time(int inode_num) {
  if (!ensure_mounted("update_file_access_time")) {
    return;
  }

  file_manager.update_file_access_time(inode_num);
}

// 更新文件修改时间
void FileSystem::update_file_modification_time(int inode_num) {
  if (!ensure_mounted("update_file_modification_time")) {
    return;
  }

  file_manager.update_file_modification_time(inode_num);
}

// 从数据块读取数据到缓冲区
bool FileSystem::read_data_from_blocks(const std::vector<int>& blocks,
                                       int offset, char* buffer, int size) {
  if (!ensure_mounted("read_data_from_blocks")) {
    return false;
  }

  return file_manager.read_data_from_blocks(blocks, offset, buffer, size);
}

// 将缓冲区数据写入到数据块
bool FileSystem::write_data_to_blocks(const std::vector<int>& blocks,
                                      int offset, const char* buffer,
                                      int size) {
  if (!ensure_mounted("write_data_to_blocks")) {
    return false;
  }

  return file_manager.write_data_to_blocks(blocks, offset, buffer, size);
}

// 分配一个新的文件描述符
int FileSystem::allocate_file_descriptor() {
  if (!ensure_mounted("allocate_file_descriptor")) {
    return -1;
  }

  return file_manager.allocate_file_descriptor();
}

// 释放文件描述符
void FileSystem::free_file_descriptor(int fd) {
  if (!ensure_mounted("free_file_descriptor")) {
    return;
  }

  file_manager.free_file_descriptor(fd);
}

// 辅助方法，用于分配和初始化文件inode
int FileSystem::allocate_file_inode(const std::string& filename) {
  int new_inode;
  if (!inode_manager.allocate_inode(new_inode)) {
    ErrorHandler::log_error(ERROR_NO_FREE_INODES,
                            "Failed to allocate inode for file: " + filename);
    return -1;
  }

  Inode inode;
  FileOperationsUtils::initialize_new_inode(inode);

  if (!inode_manager.write_inode(new_inode, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to write inode for file: " + filename);
    inode_manager.free_inode(new_inode);
    return -1;
  }

  return new_inode;
}

bool FileSystem::ensure_root_directory() {
  const int root_inode_num = 0;

  if (!inode_manager.is_inode_allocated(root_inode_num)) {
    int allocated_inode = -1;
    if (!inode_manager.allocate_inode(allocated_inode)) {
      ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to allocate root inode");
      return false;
    }

    if (allocated_inode != root_inode_num) {
      ErrorHandler::log_error(
          ERROR_INVALID_INODE,
          "Unexpected root inode index: " + std::to_string(allocated_inode));
      return false;
    }
  }

  Inode root_inode;
  if (!inode_manager.read_inode(root_inode_num, root_inode)) {
    return false;
  }

  const int directory_mode = FILE_TYPE_DIRECTORY | FILE_PERMISSION_READ |
                             FILE_PERMISSION_WRITE | FILE_PERMISSION_EXECUTE;
  bool inode_updated = false;

  if ((root_inode.mode & FILE_TYPE_DIRECTORY) == 0) {
    FileOperationsUtils::initialize_new_inode(root_inode, directory_mode, 2);
    inode_updated = true;
  } else {
    const int required_permissions =
        FILE_PERMISSION_READ | FILE_PERMISSION_WRITE | FILE_PERMISSION_EXECUTE;
    if ((root_inode.mode & required_permissions) != required_permissions) {
      root_inode.mode |= required_permissions;
      inode_updated = true;
    }

    if (root_inode.link_count < 2) {
      root_inode.link_count = 2;
      inode_updated = true;
    }
  }

  if (inode_updated) {
    if (!inode_manager.write_inode(root_inode_num, root_inode)) {
      return false;
    }
  }

  std::vector<DirectoryEntry> entries;
  bool read_ok = read_directory(root_inode_num, entries);
  bool needs_write = !read_ok;

  if (!read_ok) {
    entries.clear();
  }

  bool has_dot = false;
  bool has_dotdot = false;

  for (DirectoryEntry& entry : entries) {
    if (entry.name_length == 1 && strncmp(entry.name, ".", 1) == 0) {
      has_dot = true;
      if (entry.inode_number != root_inode_num) {
        entry.inode_number = root_inode_num;
        needs_write = true;
      }
    } else if (entry.name_length == 2 && strncmp(entry.name, "..", 2) == 0) {
      has_dotdot = true;
      if (entry.inode_number != root_inode_num) {
        entry.inode_number = root_inode_num;
        needs_write = true;
      }
    }
  }

  if (!has_dot) {
    DirectoryEntry dot;
    memset(&dot, 0, sizeof(DirectoryEntry));
    dot.inode_number = root_inode_num;
    strncpy(dot.name, ".", sizeof(dot.name) - 1);
    dot.name_length = 1;
    entries.insert(entries.begin(), dot);
    has_dot = true;
    needs_write = true;
  }

  if (!has_dotdot) {
    DirectoryEntry dotdot;
    memset(&dotdot, 0, sizeof(DirectoryEntry));
    dotdot.inode_number = root_inode_num;
    strncpy(dotdot.name, "..", sizeof(dotdot.name) - 1);
    dotdot.name_length = 2;
    auto insert_pos = has_dot ? entries.begin() + 1 : entries.begin();
    entries.insert(insert_pos, dotdot);
    needs_write = true;
  }

  if (!needs_write) {
    return true;
  }

  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(root_inode_num, blocks)) {
    return false;
  }

  if (blocks.empty()) {
    if (!inode_manager.allocate_data_blocks(root_inode_num, 1, blocks)) {
      return false;
    }
  }

  return write_directory(root_inode_num, entries);
}

bool FileSystem::load_superblock() {
  char buffer[BLOCK_SIZE];
  if (!disk.read_block(0, buffer)) {
    ErrorHandler::log_error(ERROR_IO_ERROR, "Failed to read superblock");
    return false;
  }

  memcpy(&superblock, buffer, sizeof(Superblock));

  if (superblock.magic_number != MAGIC_NUMBER) {
    ErrorHandler::log_error(ERROR_INVALID_ARGUMENT,
                            "Invalid file system format");
    return false;
  }

  layout = disk.calculate_layout();
  return true;
}

bool FileSystem::initialize_after_open() {
  if (!load_superblock()) {
    return false;
  }

  if (!inode_manager.initialize(layout)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to initialize inode manager");
    return false;
  }

  if (!ensure_root_directory()) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to initialize root directory");
    return false;
  }

  return true;
}

bool FileSystem::ensure_mounted(const char* operation) const {
  if (mounted) {
    return true;
  }

  std::string op_name = operation ? operation : "operation";
  ErrorHandler::log_error(
      ERROR_NOT_MOUNTED,
      op_name + " requires a mounted file system to proceed");
  return false;
}

void FileSystem::close_all_files() {
  std::vector<int> open_fds;
  open_fds.reserve(file_descriptors.size());
  for (const auto& pair : file_descriptors) {
    open_fds.push_back(pair.first);
  }

  for (int fd : open_fds) {
    close_file_internal(fd);
  }
}

std::shared_lock<std::shared_mutex> FileSystem::acquire_shared_lock() const {
  return std::shared_lock<std::shared_mutex>(fs_mutex_);
}

std::unique_lock<std::shared_mutex> FileSystem::acquire_unique_lock() const {
  return std::unique_lock<std::shared_mutex>(fs_mutex_);
}

// 使用PathUtils验证和解析路径的辅助方法
bool FileSystem::validate_and_parse_path(const std::string& path,
                                         std::string& filename,
                                         std::string& directory) {
  return PathUtilsExtended::extract_filename_and_directory(path, filename,
                                                           directory);
}
