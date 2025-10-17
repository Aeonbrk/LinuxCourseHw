// ==============================================================================
// @file   path_manager.h
// @brief  路径管理模块，处理路径解析、验证和inode查找
// ==============================================================================

#pragma once
#include <string>
#include <vector>

#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/path_utils.h"
#include "disk_simulator.h"
#include "inode_manager.h"

/**
 * @class PathManager
 * @brief 路径管理器，负责处理路径解析、验证和inode查找。
 */
class PathManager {
 public:
  /**
   * @brief 构造函数。
   * @param disk DiskSimulator对象的引用。
   * @param inode_manager InodeManager对象的引用。
   */
  PathManager(DiskSimulator& disk, InodeManager& inode_manager);

  /**
   * @brief 获取路径的父目录路径。
   * @param path 输入路径。
   * @return std::string 父目录路径。
   */
  std::string get_parent_path(const std::string& path);

  /**
   * @brief 获取路径的基本名称（文件名或目录名）。
   * @param path 输入路径。
   * @return std::string 基本名称。
   */
  std::string get_basename(const std::string& path);

  /**
   * @brief 解析路径字符串为路径组件列表。
   * @param path 要解析的路径。
   * @param components 用于存储路径组件的向量。
   * @return bool 解析成功返回true，否则返回false。
   */
  bool parse_path(const std::string& path,
                  std::vector<std::string>& components);

  /**
   * @brief 根据路径查找对应的inode号。
   * @param path 要查找的路径。
   * @return int 找到的inode号，失败返回-1。
   */
  int find_inode(const std::string& path);

  /**
   * @brief 在指定目录中查找文件或子目录的inode号。
   * @param parent_inode 父目录的inode号。
   * @param name 要查找的文件或目录名。
   * @return int 找到的inode号，失败返回-1。
   */
  int find_inode_in_directory(int parent_inode, const std::string& name);

  /**
   * @brief 验证并解析路径。
   * @param path 输入路径。
   * @param filename 输出文件名。
   * @param directory 输出目录名。
   * @return bool 验证和解析成功返回true，否则返回false。
   */
  bool validate_and_parse_path(const std::string& path, std::string& filename,
                               std::string& directory);

  /**
   * @brief 检查文件是否存在。
   * @param path 文件路径。
   * @return bool 存在返回true，否则返回false。
   */
  bool file_exists(const std::string& path);

 private:
  DiskSimulator& disk;          ///< 磁盘模拟器引用
  InodeManager& inode_manager;  ///< Inode管理器引用

  bool load_directory_inode(int inode_num, Inode& inode,
                            const std::string& context_hint);
};