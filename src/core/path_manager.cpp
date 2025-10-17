// ==============================================================================
// @file   path_manager.cpp
// @brief  路径管理模块的实现
// ==============================================================================

#include "path_manager.h"

#include <cstring>
#include <sstream>
#include <string>

/**
 * @brief 构造函数。
 * @param disk DiskSimulator对象的引用。
 * @param inode_manager InodeManager对象的引用。
 */
PathManager::PathManager(DiskSimulator& disk, InodeManager& inode_manager)
    : disk(disk), inode_manager(inode_manager) {
}

/**
 * @brief 获取路径的父目录路径。
 * @param path 输入路径。
 * @return std::string 父目录路径。
 */
std::string PathManager::get_parent_path(const std::string& path) {
  if (path == "/" || path.empty()) {
    return "/";
  }

  // 处理相对路径
  std::string p = path;
  if (path[0] != '/') {
    p = "/" + path;
  }

  size_t pos = p.find_last_of('/');
  if (pos == 0) {
    return "/";
  }

  return p.substr(0, pos);
}

/**
 * @brief 获取路径的基本名称（文件名或目录名）。
 * @param path 输入路径。
 * @return std::string 基本名称。
 */
std::string PathManager::get_basename(const std::string& path) {
  if (path == "/" || path.empty()) {
    return "";
  }

  // 处理相对路径
  std::string p = path;
  if (path[0] != '/') {
    p = "/" + path;
  }

  size_t pos = p.find_last_of('/');
  if (pos == std::string::npos) {
    return path;
  }

  return p.substr(pos + 1);
}

/**
 * @brief 解析路径字符串为路径组件列表。
 * @param path 要解析的路径。
 * @param components 用于存储路径组件的向量。
 * @return bool 解析成功返回true，否则返回false。
 */
bool PathManager::parse_path(const std::string& path,
                             std::vector<std::string>& components) {
  components.clear();

  if (path.empty()) {
    ErrorHandler::log_error(ERROR_INVALID_PATH, "Empty path provided");
    return false;
  }

  if (path == "/") {
    return true;
  }

  std::string p = path;
  // 如果路径不以'/'开头，说明是相对路径，我们假设相对于根目录
  if (path[0] != '/') {
    p = "/" + path;
  }

  p = p.substr(1);  // 去掉开头的 '/'
  std::stringstream ss(p);
  std::string component;

  while (std::getline(ss, component, '/')) {
    if (!component.empty()) {
      components.push_back(component);
    }
  }

  return true;
}

/**
 * @brief 根据路径查找对应的inode号。
 * @param path 要查找的路径。
 * @return int 找到的inode号，失败返回-1。
 */
int PathManager::find_inode(const std::string& path) {
  // 根据需要检查mounted状态

  if (path == "/") {
    return 0;  // 根目录的inode是0
  }

  std::vector<std::string> components;
  if (!parse_path(path, components)) {
    ErrorHandler::log_error(ERROR_INVALID_PATH, "Invalid path: " + path);
    return -1;
  }

  int current_inode = 0;  // 从根目录开始

  for (const std::string& component : components) {
    current_inode = find_inode_in_directory(current_inode, component);
    if (current_inode == -1) {
      return -1;
    }
  }

  return current_inode;
}

// 在指定目录中查找文件或子目录的inode号
int PathManager::find_inode_in_directory(int parent_inode,
                                         const std::string& name) {
  // 读取父目录的inode信息
  Inode parent_inode_info;
  if (!load_directory_inode(parent_inode, parent_inode_info,
                            std::to_string(parent_inode))) {
    return -1;
  }

  // 获取目录数据块
  std::vector<int> blocks;
  if (!inode_manager.get_data_blocks(parent_inode, blocks)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to get data blocks for directory inode=" +
                                std::to_string(parent_inode));
    return -1;
  }

  // 读取目录数据
  char buffer[BLOCK_SIZE];
  for (int block_num : blocks) {
    if (!disk.read_block(block_num, buffer)) {
      ErrorHandler::log_error(
          ERROR_IO_ERROR,
          "Failed to read directory block: " + std::to_string(block_num));
      return -1;
    }

    DirectoryEntry* entry = reinterpret_cast<DirectoryEntry*>(buffer);
    int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < max_entries; i++) {
      if (entry[i].name_length > 0 &&
          strcmp(entry[i].name, name.c_str()) == 0) {
        return entry[i].inode_number;
      }
    }
  }

  return -1;
}

// 使用PathUtils验证和解析路径的辅助方法
bool PathManager::validate_and_parse_path(const std::string& path,
                                          std::string& filename,
                                          std::string& directory) {
  if (ErrorHandler::is_error(PathUtils::validate_path(path))) {
    ErrorHandler::log_error(ERROR_INVALID_PATH, "Invalid path: " + path);
    return false;
  }

  filename = PathUtils::extract_filename(path);
  directory = PathUtils::extract_directory(path);

  if (directory == ".") {
    directory = "/";
  }

  if (directory.empty()) {
    directory = "/";
  }

  return true;
}

// 检查文件是否存在
bool PathManager::file_exists(const std::string& path) {
  return find_inode(path) != -1;
}

bool PathManager::load_directory_inode(int inode_num, Inode& inode,
                                       const std::string& context_hint) {
  if (!inode_manager.read_inode(inode_num, inode)) {
    ErrorHandler::log_error(ERROR_IO_ERROR,
                            "Failed to read inode: " + context_hint);
    return false;
  }

  if (!(inode.mode & FILE_TYPE_DIRECTORY)) {
    ErrorHandler::log_error(ERROR_NOT_A_DIRECTORY,
                            "Inode is not a directory: " + context_hint);
    return false;
  }

  return true;
}