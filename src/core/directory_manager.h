// ==============================================================================
// @file   directory_manager.h
// @brief  目录管理模块，处理目录的创建、删除、列出和管理
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
#include "inode_manager.h"
#include "path_manager.h"

/**
 * @class DirectoryManager
 * @brief 目录管理器，负责处理目录的创建、删除、列出和管理。
 */
class DirectoryManager {
 public:
  /**
   * @brief 构造函数。
   * @param disk DiskSimulator对象的引用。
   * @param inode_manager InodeManager对象的引用。
   * @param path_manager PathManager对象的引用。
   */
  DirectoryManager(DiskSimulator& disk, InodeManager& inode_manager,
                   PathManager& path_manager);

  /**
   * @brief 创建新目录，分配inode并初始化目录结构。
   * @param path 要创建的目录路径。
   * @return bool 创建成功返回true，否则返回false。
   */
  bool create_directory(const std::string& path);

  /**
   * @brief 列出目录内容，返回目录条目列表。
   * @param path 要列出的目录路径。
   * @param entries 用于存储目录条目的向量。
   * @return bool 列出成功返回true，否则返回false。
   */
  bool list_directory(const std::string& path,
                      std::vector<DirectoryEntry>& entries);

  /**
   * @brief 删除目录，检查是否为空后释放资源。
   * @param path 要删除的目录路径。
   * @return bool 删除成功返回true，否则返回false。
   */
  bool remove_directory(const std::string& path);

  /**
   * @brief 读取目录内容，返回目录条目列表。
   * @param inode_num 目录的inode号。
   * @param entries 用于存储目录条目的向量。
   * @return bool 读取成功返回true，否则返回false。
   */
  bool read_directory(int inode_num, std::vector<DirectoryEntry>& entries);

  /**
   * @brief 写入目录内容到磁盘。
   * @param inode_num 目录的inode号。
   * @param entries 要写入的目录条目向量。
   * @return bool 写入成功返回true，否则返回false。
   */
  bool write_directory(int inode_num,
                       const std::vector<DirectoryEntry>& entries);

  /**
   * @brief 在目录中添加新的条目。
   * @param dir_inode 目录的inode号。
   * @param name 要添加的条目名称。
   * @param inode_num 对应的inode号。
   * @return bool 添加成功返回true，否则返回false。
   */
  bool add_directory_entry(int dir_inode, const std::string& name,
                           int inode_num);

  /**
   * @brief 从目录中移除条目。
   * @param dir_inode 目录的inode号。
   * @param name 要移除的条目名称。
   * @return bool 移除成功返回true，否则返回false。
   */
  bool remove_directory_entry(int dir_inode, const std::string& name);

 private:
  DiskSimulator& disk;          ///< 磁盘模拟器引用
  InodeManager& inode_manager;  ///< Inode管理器引用
  PathManager& path_manager;    ///< 路径管理器引用

  bool load_directory_inode(int inode_num, Inode& inode);
  int find_entry_index(const std::vector<DirectoryEntry>& entries,
                       const std::string& name) const;
};