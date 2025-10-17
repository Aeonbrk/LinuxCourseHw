// ==============================================================================
// @file   file_manager.cpp
// @brief  文件管理模块的实现
// ==============================================================================

#include "file_manager.h"

#include <algorithm>
#include <string>

// 构造函数
FileManager::FileManager(DiskSimulator& disk, InodeManager& inode_manager,
                         PathManager& path_manager,
                         DirectoryManager& directory_manager,
                         std::map<int, FileDescriptor>& file_descriptors,
                         int& next_fd)
    : disk(disk),
      inode_manager(inode_manager),
      path_manager(path_manager),
      directory_manager(directory_manager),
      file_descriptors(file_descriptors),
      next_fd(next_fd) {
}

// 创建新文件，分配inode并在父目录中添加条目
int FileManager::create_file(const std::string& path, int mode) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  // 检查文件是否已存在
  if (file_exists(path)) {
    ErrorHandler::log_error(ERROR_FILE_ALREADY_EXISTS,
                            "File already exists: " + path);
    return -1;
  }

  // 验证并解析路径使用辅助方法
  std::string filename, directory;
  if (!path_manager.validate_and_parse_path(path, filename, directory)) {
    return -1;
  }

  // 查找父目录inode
  int parent_inode = path_manager.find_inode(directory);
  if (parent_inode == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND,
                            "Parent directory not found: " + directory);
    return -1;
  }

  // 分配并初始化文件inode使用辅助方法
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

  // 在父目录中添加目录条目
  if (!directory_manager.add_directory_entry(parent_inode, filename,
                                             new_inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to append directory entry: " + filename);
    inode_manager.free_inode(new_inode);
    return -1;
  }

  return new_inode;
}

// 删除文件，从父目录中移除条目并释放inode和数据块
bool FileManager::delete_file(const std::string& path) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  int inode_num = path_manager.find_inode(path);
  if (inode_num == -1) {
    ErrorHandler::log_error(ERROR_FILE_NOT_FOUND, "File not found: " + path);
    return false;
  }

  Inode inode;
  if (!load_regular_file_inode(inode_num, inode, path)) {
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
  // 这需要通过DirectoryManager来实现
  // 目前，我们假设它在主FileSystem类中可用

  // 释放inode和数据块
  return inode_manager.free_inode(inode_num);
}

// 打开文件，分配文件描述符
int FileManager::open_file(const std::string& path, int mode) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  int inode_num = path_manager.find_inode(path);
  if (inode_num == -1) {
    if (mode & OPEN_MODE_CREATE) {
      inode_num =
          create_file(path, FILE_PERMISSION_READ | FILE_PERMISSION_WRITE);
      if (inode_num == -1) {
        return -1;
      }
    } else {
      ErrorHandler::log_error(ERROR_FILE_NOT_FOUND, "File not found: " + path);
      return -1;
    }
  }

  // 分配文件描述符
  int fd;
  if (!allocate_file_descriptor(inode_num, mode, fd)) {
    return -1;
  }

  if (mode & OPEN_MODE_APPEND) {
    Inode inode;
    if (!inode_manager.read_inode(inode_num, inode)) {
      ErrorHandler::log_error(ERROR_IO_ERROR,
                              "Failed to read inode for append: " + path);
      free_file_descriptor(fd);
      return -1;
    }
    file_descriptors[fd].position = inode.size;
  }

  update_file_access_time(inode_num);
  return fd;
}

// 关闭文件，释放文件描述符并更新修改时间
bool FileManager::close_file(int fd) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  auto it = file_descriptors.find(fd);
  if (it == file_descriptors.end()) {
    ErrorHandler::log_error(ERROR_INVALID_FILE_DESCRIPTOR,
                            "Invalid file descriptor: " + std::to_string(fd));
    return false;
  }

  update_file_modification_time(it->second.inode_num);
  free_file_descriptor(fd);
  return true;
}

// 从文件中读取数据
int FileManager::read_file(int fd, char* buffer, int size) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  FileDescriptor desc;
  if (!get_file_descriptor(fd, desc)) {
    return -1;
  }

  if (!(desc.mode & OPEN_MODE_READ)) {
    ErrorHandler::log_error(
        ERROR_INVALID_ARGUMENT,
        "File not opened for reading: fd=" + std::to_string(fd));
    return -1;
  }

  Inode inode;
  if (!inode_manager.read_inode(desc.inode_num, inode)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR, "Failed to read inode for fd=" + std::to_string(fd));
    return -1;
  }

  // 检查文件大小
  if (desc.position >= inode.size) {
    return 0;  // 文件结束
  }

  int bytes_to_read = std::min(size, inode.size - desc.position);

  // 获取数据块
  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(desc.inode_num, blocks)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to get data blocks for fd=" + std::to_string(fd));
    return -1;
  }

  if (!read_data_from_blocks(blocks, desc.position, buffer, bytes_to_read)) {
    return -1;
  }

  // 更新文件位置
  file_descriptors[fd].position += bytes_to_read;
  update_file_access_time(desc.inode_num);

  return bytes_to_read;
}

// 向文件中写入数据
int FileManager::write_file(int fd, const char* buffer, int size) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  FileDescriptor desc;
  if (!get_file_descriptor(fd, desc)) {
    return -1;
  }

  if (!(desc.mode & OPEN_MODE_WRITE)) {
    ErrorHandler::log_error(
        ERROR_INVALID_ARGUMENT,
        "File not opened for writing: fd=" + std::to_string(fd));
    return -1;
  }

  Inode inode;
  if (!inode_manager.read_inode(desc.inode_num, inode)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR, "Failed to read inode for fd=" + std::to_string(fd));
    return -1;
  }

  // 计算需要的块数
  int current_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  int required_blocks = (desc.position + size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  int additional_blocks = required_blocks - current_blocks;

  // 分配额外的块（如果需要）
  if (additional_blocks > 0) {
    std::vector<int> new_blocks;
    if (!inode_manager.allocate_data_blocks(desc.inode_num, additional_blocks,
                                            new_blocks)) {
      ErrorHandler::log_error(
          ERROR_NO_FREE_BLOCKS,
          "Failed to allocate data blocks for fd=" + std::to_string(fd));
      return -1;
    }
    // 重新读取inode，因为allocate_data_blocks已经修改了它
    if (!inode_manager.read_inode(desc.inode_num, inode)) {
      ErrorHandler::log_error(
          ERROR_IO_ERROR,
          "Failed to refresh inode for fd=" + std::to_string(fd));
      return -1;
    }
  }

  // 获取所有数据块
  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(desc.inode_num, blocks)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to get data blocks for fd=" + std::to_string(fd));
    return -1;
  }

  // 写入数据
  if (!write_data_to_blocks(blocks, desc.position, buffer, size)) {
    return -1;
  }

  // 更新文件大小和位置
  int new_size = std::max(inode.size, desc.position + size);
  inode.size = new_size;
  inode.modification_time = time(nullptr);

  if (!inode_manager.write_inode(desc.inode_num, inode)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to update inode after write for fd=" + std::to_string(fd));
    return -1;
  }

  file_descriptors[fd].position += size;
  return size;
}

// 设置文件读写位置
bool FileManager::seek_file(int fd, int position) {
  // 检查文件系统是否已挂载
  // 这需要在主文件系统类中进行检查

  FileDescriptor desc;
  if (!get_file_descriptor(fd, desc)) {
    return false;
  }

  Inode inode;
  if (!inode_manager.read_inode(desc.inode_num, inode)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to read inode for seek: fd=" + std::to_string(fd));
    return false;
  }

  if (position < 0 || position > inode.size) {
    ErrorHandler::log_error(
        ERROR_INVALID_ARGUMENT,
        "Invalid seek position: " + std::to_string(position));
    return false;
  }

  file_descriptors[fd].position = position;
  return true;
}

// 检查文件是否存在
bool FileManager::file_exists(const std::string& path) {
  return path_manager.find_inode(path) != -1;
}

// 分配文件描述符
bool FileManager::allocate_file_descriptor(int inode_num, int mode, int& fd) {
  fd = allocate_file_descriptor();
  if (fd == -1) {
    ErrorHandler::log_error(ERROR_INVALID_FILE_DESCRIPTOR,
                            "No available file descriptors");
    return false;
  }

  FileDescriptor desc;
  desc.inode_num = inode_num;
  desc.mode = mode;
  desc.position = 0;
  desc.open = true;

  file_descriptors[fd] = desc;
  return true;
}

// 获取文件描述符信息
bool FileManager::get_file_descriptor(int fd, FileDescriptor& desc) {
  auto it = file_descriptors.find(fd);
  if (it == file_descriptors.end() || !it->second.open) {
    ErrorHandler::log_error(
        ERROR_INVALID_FILE_DESCRIPTOR,
        "File descriptor not open: fd=" + std::to_string(fd));
    return false;
  }

  desc = it->second;
  return true;
}

// 更新文件访问时间
void FileManager::update_file_access_time(int inode_num) {
  Inode inode;
  if (inode_manager.read_inode(inode_num, inode)) {
    inode.access_time = time(nullptr);
    inode_manager.write_inode(inode_num, inode);
  }
}

// 更新文件修改时间
void FileManager::update_file_modification_time(int inode_num) {
  Inode inode;
  if (inode_manager.read_inode(inode_num, inode)) {
    inode.modification_time = time(nullptr);
    inode_manager.write_inode(inode_num, inode);
  }
}

// 分配一个新的文件描述符
int FileManager::allocate_file_descriptor() {
  // 从3开始，0、1、2保留给标准输入、输出、错误
  while (file_descriptors.find(next_fd) != file_descriptors.end()) {
    next_fd++;
    if (next_fd > 1024) {
      next_fd = 3;
    }
  }

  return next_fd++;
}

// 释放文件描述符
void FileManager::free_file_descriptor(int fd) {
  auto it = file_descriptors.find(fd);
  if (it != file_descriptors.end()) {
    it->second.open = false;
    file_descriptors.erase(it);
  }
}

// 从数据块读取数据到缓冲区
bool FileManager::read_data_from_blocks(const std::vector<int>& blocks,
                                        int offset, char* buffer, int size) {
  if (!FileOperationsUtils::read_data_from_blocks(disk, blocks, offset, buffer,
                                                  size)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to read data blocks at offset " + std::to_string(offset));
    return false;
  }
  return true;
}

// 将缓冲区数据写入到数据块
bool FileManager::write_data_to_blocks(const std::vector<int>& blocks,
                                       int offset, const char* buffer,
                                       int size) {
  if (!FileOperationsUtils::write_data_to_blocks(disk, blocks, offset, buffer,
                                                 size)) {
    ErrorHandler::log_error(
        ERROR_IO_ERROR,
        "Failed to write data blocks at offset " + std::to_string(offset));
    return false;
  }
  return true;
}

// 分配文件inode
int FileManager::allocate_file_inode(const std::string& filename) {
  int new_inode;
  if (!inode_manager.allocate_inode(new_inode)) {
    ErrorHandler::log_error(ERROR_NO_FREE_INODES,
                            "Failed to allocate inode for file: " + filename);
    return -1;
  }

  Inode inode;
  FileOperationsUtils::initialize_new_inode(
      inode);  // Use default parameters for files

  if (!inode_manager.write_inode(new_inode, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to write inode for file: " + filename);
    inode_manager.free_inode(new_inode);
    return -1;
  }

  return new_inode;
}

bool FileManager::load_regular_file_inode(int inode_num, Inode& inode,
                                          const std::string& context_path) {
  if (!inode_manager.read_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to read inode for: " + context_path);
    return false;
  }

  if (inode.mode & FILE_TYPE_DIRECTORY) {
    ErrorHandler::log_error(ERROR_IS_A_DIRECTORY,
                            "Path is a directory: " + context_path);
    return false;
  }

  return true;
}