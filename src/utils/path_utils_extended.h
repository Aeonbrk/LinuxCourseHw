#pragma once
#include <string>
#include <vector>

#include "../utils/common.h"
#include "../utils/error_codes.h"
#include "../utils/error_handler.h"
#include "../utils/path_utils.h"

/**
 * @class PathUtilsExtended
 * @brief 扩展路径操作工具类，提供更高级的路径处理功能。
 * 
 * 此类提供静态方法，用于处理各种路径相关的操作，
 * 以减少代码重复并简化路径处理逻辑。
 */
class PathUtilsExtended {
public:
    /**
     * @brief 验证并规范化路径。
     * @param path 输入路径。
     * @param[out] normalized_path 规范化后的路径。
     * @return bool 验证和规范化成功返回true，否则返回false。
     */
    static bool validate_and_normalize_path(const std::string& path, 
                                          std::string& normalized_path);

    /**
     * @brief 从完整路径中提取文件名和目录路径。
     * @param path 完整路径。
     * @param[out] filename 提取的文件名。
     * @param[out] directory 提取的目录路径。
     * @return bool 提取成功返回true，否则返回false。
     */
    static bool extract_filename_and_directory(const std::string& path,
                                             std::string& filename,
                                             std::string& directory);
};