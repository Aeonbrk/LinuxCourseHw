#pragma once
#include <string>
#include "../utils/error_codes.h"

/**
 * @class PathUtils
 * @brief 提供路径操作的静态工具方法。
 *
 * 用于处理文件系统路径的验证、解析和规范化。
 */
class PathUtils {
 public:
  /**
   * @brief 验证路径格式是否有效。
   * @param path 要验证的路径字符串。
   * @return ErrorCode 如果路径有效返回SUCCESS，否则返回错误码。
   */
  static ErrorCode validate_path(const std::string& path);

  /**
   * @brief 从完整路径中提取文件名。
   * @param path 完整路径字符串。
   * @return std::string 提取的文件名。如果路径无效则返回空字符串。
   */
  static std::string extract_filename(const std::string& path);

  /**
   * @brief 从完整路径中提取目录路径。
   * @param path 完整路径字符串。
   * @return std::string 目录路径。如果路径无效则返回空字符串。
   */
  static std::string extract_directory(const std::string& path);

  /**
   * @brief 检查路径是否为绝对路径。
   * @param path 要检查的路径字符串。
   * @return bool 如果是绝对路径返回true，否则返回false。
   */
  static bool is_absolute_path(const std::string& path);

  /**
   * @brief 规范化路径，统一使用'/'作为分隔符。
   * @param path 要规范化的路径字符串。
   * @return std::string 规范化后的路径字符串。
   */
  static std::string normalize_path(const std::string& path);
};