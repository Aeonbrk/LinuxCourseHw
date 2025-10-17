// ==============================================================================
// @file   path_utils_extended.cpp
// @brief  扩展路径操作工具类的实现
// ==============================================================================

#include "path_utils_extended.h"

/**
 * @brief 验证并规范化路径。
 * @param path 输入路径。
 * @param[out] normalized_path 规范化后的路径。
 * @return bool 验证和规范化成功返回true，否则返回false。
 */
bool PathUtilsExtended::validate_and_normalize_path(const std::string& path, 
                                                  std::string& normalized_path) {
    if (ErrorHandler::is_error(PathUtils::validate_path(path))) {
        ErrorHandler::log_error(ERROR_INVALID_PATH, "Invalid path: " + path);
        return false;
    }
    
    normalized_path = PathUtils::normalize_path(path);
    return true;
}

/**
 * @brief 从完整路径中提取文件名和目录路径。
 * @param path 完整路径。
 * @param[out] filename 提取的文件名。
 * @param[out] directory 提取的目录路径。
 * @return bool 提取成功返回true，否则返回false。
 */
bool PathUtilsExtended::extract_filename_and_directory(const std::string& path,
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