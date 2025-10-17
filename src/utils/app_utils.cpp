// ==============================================================================
// @file   app_utils.cpp
// @brief  应用程序实用工具类的实现
// ==============================================================================

#include "app_utils.h"
#include <string>
#include <stdexcept>  // Required for std::invalid_argument and std::out_of_range

/**
 * @brief 验证命令行参数。
 * @param argc 参数数量。
 * @param argv 参数数组。
 * @param min_args 最少需要的参数数量。
 * @return bool 参数有效返回true，否则返回false。
 */
bool AppUtils::validate_args(int argc, char* argv[], int min_args) {
    (void)argv;  // 抑制未使用的参数警告
    return argc >= min_args;
}

/**
 * @brief 尝试将字符串转换为整数。
 * @param str 要转换的字符串。
 * @param[out] result 转换后的整数。
 * @return bool 转换成功返回true，否则返回false。
 */
bool AppUtils::try_stoi(const std::string& str, int& result) {
    try {
        result = std::stoi(str);
        return true;
    } catch (const std::invalid_argument&) {
        ErrorHandler::log_error(ERROR_INVALID_ARGUMENT, "Invalid number format: " + str);
        return false;
    } catch (const std::out_of_range&) {
        ErrorHandler::log_error(ERROR_INVALID_ARGUMENT, "Number out of range: " + str);
        return false;
    }
}