// ==============================================================================
// @file   filesystem.h
// @brief  文件系统高级API
// ==============================================================================

#pragma once
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "../utils/block_utils.h"
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/file_operations_utils.h"
#include "../utils/path_utils.h"
#include "../utils/path_utils_extended.h"
#include "bitmap_manager.h"
#include "block_manager.h"
#include "directory_manager.h"
#include "disk_simulator.h"
#include "file_manager.h"
#include "inode_manager.h"
#include "path_manager.h"

// 文件系统高级API
class FileSystem {
 public:
  // 构造函数
  FileSystem();
  // 析构函数
  ~FileSystem();

  // 挂载磁盘
  bool mount(const std::string& disk_path);
  // 卸载磁盘
  bool unmount();
  // 格式化文件系统
  bool format();
  // 检查文件系统是否已挂载
  bool is_mounted() const;

  // 创建文件
  int create_file(const std::string& path, int mode);
  // 删除文件
  bool delete_file(const std::string& path);
  // 检查文件是否存在
  bool file_exists(const std::string& path);

  // 打开文件
  int open_file(const std::string& path, int mode);
  // 关闭文件
  bool close_file(int fd);
  // 读取文件
  int read_file(int fd, char* buffer, int size);
  // 写入文件
  int write_file(int fd, const char* buffer, int size);
  // 移动文件指针
  bool seek_file(int fd, int position);

  // 创建目录
  bool create_directory(const std::string& path);
  // 列出目录内容
  bool list_directory(const std::string& path,
                      std::vector<DirectoryEntry>& entries);
  // 删除目录
  bool remove_directory(const std::string& path);

  // 获取磁盘信息
  bool get_disk_info(std::string& info);

  // 判断路径是否为目录
  bool is_directory(const std::string& path);

  // 获取父路径
  std::string get_parent_path(const std::string& path);
  // 获取基本名称
  std::string get_basename(const std::string& path);

 private:
  DiskSimulator disk;                              // 磁盘模拟器
  Superblock superblock;                           // 超级块
  InodeManager inode_manager;                      // Inode管理器
  DiskLayout layout;                               // 磁盘布局
  bool mounted;                                    // 挂载标志
  std::map<int, FileDescriptor> file_descriptors;  // 打开文件描述符表
  int next_fd;                                     // 下一个可用的文件描述符

  // 新增模块管理器
  PathManager path_manager;            // 路径管理器
  DirectoryManager directory_manager;  // 目录管理器
  FileManager file_manager;            // 文件管理器

  // --- 私有辅助函数 ---
  // 这些函数仅供内部使用，由公共方法调用
  bool parse_path(const std::string& path,
                  std::vector<std::string>& components);
  // 查找路径对应的inode
  int find_inode(const std::string& path);
  // 在目录中查找inode
  int find_inode_in_directory(int parent_inode, const std::string& name);
  // 添加目录项
  bool add_directory_entry(int dir_inode, const std::string& name,
                           int inode_num);
  // 删除目录项
  bool remove_directory_entry(int dir_inode, const std::string& name);
  // 读取目录
  bool read_directory(int inode_num, std::vector<DirectoryEntry>& entries);
  // 写入目录
  bool write_directory(int inode_num,
                       const std::vector<DirectoryEntry>& entries);
  // 分配文件描述符
  bool allocate_file_descriptor(int inode_num, int mode, int& fd);
  // 获取文件描述符
  bool get_file_descriptor(int fd, FileDescriptor& desc);
  // 更新文件访问时间
  void update_file_access_time(int inode_num);
  // 更新文件修改时间
  void update_file_modification_time(int inode_num);
  // 从数据块读取数据
  bool read_data_from_blocks(const std::vector<int>& blocks, int offset,
                             char* buffer, int size);
  // 向数据块写入数据
  bool write_data_to_blocks(const std::vector<int>& blocks, int offset,
                            const char* buffer, int size);
  // 分配文件描述符
  int allocate_file_descriptor();
  // 释放文件描述符
  void free_file_descriptor(int fd);

  // 重构函数的辅助方法
  bool validate_and_parse_path(const std::string& path, std::string& filename,
                               std::string& directory);
  int allocate_file_inode(const std::string& filename);

  bool ensure_root_directory();
  bool load_superblock();
  bool initialize_after_open();
  bool ensure_mounted(const char* operation) const;
  void close_all_files();
  bool close_file_internal(int fd);

  /**
   * @brief 获取共享锁，用于只读操作。
   */
  [[nodiscard]] std::shared_lock<std::shared_mutex> acquire_shared_lock() const;

  /**
   * @brief 获取独占锁，用于写操作。
   */
  [[nodiscard]] std::unique_lock<std::shared_mutex> acquire_unique_lock() const;

  mutable std::shared_mutex fs_mutex_;  ///< 文件系统读写锁
};
