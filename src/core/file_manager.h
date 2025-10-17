// ==============================================================================
// @file   file_manager.h
// @brief  文件管理模块，处理文件的创建、删除、读写等操作
// ==============================================================================

#pragma once
#include <map>
#include <string>
#include <vector>

#include "../utils/block_utils.h"
#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/file_operations_utils.h"
#include "../utils/path_utils.h"
#include "disk_simulator.h"
#include "directory_manager.h"
#include "inode_manager.h"
#include "path_manager.h"

/**
 * @class FileManager
 * @brief 文件管理器，负责处理文件的创建、删除、读写等操作。
 */
class FileManager {
 public:
  /**
   * @brief 构造函数。
   * @param disk DiskSimulator对象的引用。
   * @param inode_manager InodeManager对象的引用。
   * @param path_manager PathManager对象的引用。
   */
  FileManager(DiskSimulator& disk, InodeManager& inode_manager,
              PathManager& path_manager, DirectoryManager& directory_manager,
              std::map<int, FileDescriptor>& file_descriptors, int& next_fd);

  /**
   * @brief 创建新文件，分配inode并在父目录中添加条目。
   * @param path 要创建的文件路径。
   * @param mode 文件权限模式。
   * @return int 创建的文件的inode号，失败返回-1。
   */
  int create_file(const std::string& path, int mode);

  /**
   * @brief 删除文件，从父目录中移除条目并释放inode和数据块。
   * @param path 要删除的文件路径。
   * @return bool 删除成功返回true，否则返回false。
   */
  bool delete_file(const std::string& path);

  /**
   * @brief 打开文件，分配文件描述符。
   * @param path 要打开的文件路径。
   * @param mode 打开模式。
   * @return int 文件描述符，失败返回-1。
   */
  int open_file(const std::string& path, int mode);

  /**
   * @brief 关闭文件，释放文件描述符并更新修改时间。
   * @param fd 文件描述符。
   * @return bool 关闭成功返回true，否则返回false。
   */
  bool close_file(int fd);

  /**
   * @brief 从文件中读取数据。
   * @param fd 文件描述符。
   * @param buffer 用于存储读取数据的缓冲区。
   * @param size 要读取的字节数。
   * @return int 实际读取的字节数，失败返回-1。
   */
  int read_file(int fd, char* buffer, int size);

  /**
   * @brief 向文件中写入数据。
   * @param fd 文件描述符。
   * @param buffer 包含要写入数据的缓冲区。
   * @param size 要写入的字节数。
   * @return int 实际写入的字节数，失败返回-1。
   */
  int write_file(int fd, const char* buffer, int size);

  /**
   * @brief 设置文件读写位置。
   * @param fd 文件描述符。
   * @param position 新的位置。
   * @return bool 设置成功返回true，否则返回false。
   */
  bool seek_file(int fd, int position);

  /**
   * @brief 检查文件是否存在。
   * @param path 文件路径。
   * @return bool 存在返回true，否则返回false。
   */
  bool file_exists(const std::string& path);

  /**
   * @brief 分配文件描述符。
   * @param inode_num 对应的inode号。
   * @param mode 打开模式。
   * @param fd 用于输出的文件描述符。
   * @return bool 分配成功返回true，否则返回false。
   */
  bool allocate_file_descriptor(int inode_num, int mode, int& fd);

  /**
   * @brief 获取文件描述符信息。
   * @param fd 文件描述符。
   * @param desc 用于输出的文件描述符信息。
   * @return bool 获取成功返回true，否则返回false。
   */
  bool get_file_descriptor(int fd, FileDescriptor& desc);

  /**
   * @brief 更新文件访问时间。
   * @param inode_num inode号。
   */
  void update_file_access_time(int inode_num);

  /**
   * @brief 更新文件修改时间。
   * @param inode_num inode号。
   */
  void update_file_modification_time(int inode_num);

  /**
   * @brief 分配一个新的文件描述符。
   * @return int 分配的文件描述符，失败返回-1。
   */
  int allocate_file_descriptor();

  /**
   * @brief 释放文件描述符。
   * @param fd 要释放的文件描述符。
   */
  void free_file_descriptor(int fd);

  /**
   * @brief 从数据块读取数据到缓冲区。
   * @param blocks 数据块列表。
   * @param offset 偏移量。
   * @param buffer 输出缓冲区。
   * @param size 要读取的大小。
   * @return bool 读取成功返回true，否则返回false。
   */
  bool read_data_from_blocks(const std::vector<int>& blocks, int offset,
                             char* buffer, int size);

  /**
   * @brief 将缓冲区数据写入到数据块。
   * @param blocks 数据块列表。
   * @param offset 偏移量。
   * @param buffer 输入缓冲区。
   * @param size 要写入的大小。
   * @return bool 写入成功返回true，否则返回false。
   */
  bool write_data_to_blocks(const std::vector<int>& blocks, int offset,
                            const char* buffer, int size);

  /**
   * @brief 分配文件inode。
   * @param filename 文件名。
   * @return int 分配的inode号，失败返回-1。
   */
  int allocate_file_inode(const std::string& filename);

 private:
  DiskSimulator& disk;          ///< 磁盘模拟器引用
  InodeManager& inode_manager;  ///< Inode管理器引用
  PathManager& path_manager;     ///< 路径管理器引用
  DirectoryManager& directory_manager;  ///< 目录管理器引用
  std::map<int, FileDescriptor>&
      file_descriptors;  ///< 打开文件描述符表 (引用自主类)
  int& next_fd;          ///< 下一个可用的文件描述符 (引用自主类)

  bool load_regular_file_inode(int inode_num, Inode& inode,
                               const std::string& context_path);
};