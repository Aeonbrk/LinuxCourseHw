#pragma once
#include <string>
#include <vector>

#include "core/disk_simulator.h"
#include "utils/common.h"
#include "utils/error_codes.h"
#include "utils/error_handler.h"

/**
 * @class AppUtils
 * @brief 提供应用程序级别的实用工具函数。
 * 
 * 此类提供静态方法，用于处理应用程序层面的通用操作，
 * 以减少代码重复并简化主应用程序类的逻辑。
 */
class AppUtils {
public:
    /**
     * @brief 验证命令行参数。
     * @param argc 参数数量。
     * @param argv 参数数组。
     * @param min_args 最少需要的参数数量。
     * @return bool 参数有效返回true，否则返回false。
     */
    static bool validate_args(int argc, char* argv[], int min_args);

    /**
     * @brief 尝试将字符串转换为整数。
     * @param str 要转换的字符串。
     * @param[out] result 转换后的整数。
     * @return bool 转换成功返回true，否则返回false。
     */
    static bool try_stoi(const std::string& str, int& result);
};