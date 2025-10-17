// ==============================================================================
// @file   path_utils.cpp
// @brief  路径操作的静态工具类的实现
// ==============================================================================

#include "path_utils.h"
#include <algorithm>
#include "../utils/common.h"

/**
 * @brief 验证路径格式是否有效。
 * 
 * @param path 要验证的路径字符串。
 * @return ErrorCode 如果路径有效返回SUCCESS，否则返回错误码。
 */
ErrorCode PathUtils::validate_path(const std::string& path) {
  if (path.empty() || path.length() > MAX_PATH_LENGTH) {
    return ERROR_INVALID_PATH;
  }
  
  // 检查路径中是否包含无效字符 (例如 null, newline)
  for (char c : path) {
    if (c == '\0' || c == '\n' || c == '\r') {
      return ERROR_INVALID_PATH;
    }
  }
  
  return SUCCESS;
}

/**
 * @brief 从完整路径中提取文件名。
 * 
 * @param path 完整路径字符串。
 * @return std::string 提取的文件名。如果路径无效或以'/'结尾，则返回空字符串。
 */
std::string PathUtils::extract_filename(const std::string& path) {
  if (validate_path(path) != SUCCESS) {
    return "";
  }
  
  size_t last_slash = path.find_last_of('/');
  
  if (last_slash == std::string::npos) {
    return path; // 没有找到分隔符，整个路径是文件名
  }
  
  return path.substr(last_slash + 1);
}

/**
 * @brief 从完整路径中提取目录路径。
 * 
 * @param path 完整路径字符串。
 * @return std::string 目录路径。如果路径无效则返回空字符串。
 */
std::string PathUtils::extract_directory(const std::string& path) {
  if (validate_path(path) != SUCCESS) {
    return "";
  }
  
  size_t last_slash = path.find_last_of('/');
  
  if (last_slash == std::string::npos) {
    return "."; // 没有找到分隔符，返回当前目录
  }
  
  if (last_slash == 0) {
    return "/"; // 路径为 "/" 或 "/filename"
  }
  
  return path.substr(0, last_slash);
}

/**
 * @brief 检查路径是否为绝对路径。
 * 
 * @param path 要检查的路径字符串。
 * @return bool 如果是绝对路径（以'/'开头）返回true，否则返回false。
 */
bool PathUtils::is_absolute_path(const std::string& path) {
  if (path.empty()) {
      return false;
  }
  return path[0] == '/';
}

/**
 * @brief 规范化路径。
 * 
 * - 将所有'\'替换为'/'。
 * - 将连续的多个'/'替换为单个'/'。
 * - 移除路径末尾的'/'（根目录除外）。
 * 
 * @param path 要规范化的路径字符串。
 * @return std::string 规范化后的路径字符串。
 */
std::string PathUtils::normalize_path(const std::string& path) {
  if (path.empty()) {
      return "";
  }

  std::string normalized = path;
  std::replace(normalized.begin(), normalized.end(), '\\', '/');

  // 使用 erase-remove idiom 清理重复的斜杠
  normalized.erase(std::unique(normalized.begin(), normalized.end(),
                               [](char a, char b) { return a == '/' && b == '/'; }),
                   normalized.end());

  if (normalized.length() > 1 && normalized.back() == '/') {
    normalized.pop_back();
  }
  
  return normalized;
}
