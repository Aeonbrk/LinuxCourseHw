#pragma once

#include <string>
#include "error_codes.h"

/**
 * @brief 标准化错误处理工具类。
 * 
 * 提供统一的错误处理、格式化和日志记录功能，
 * 确保整个代码库中的错误处理保持一致性。
 */
class ErrorHandler {
public:
    /**
     * @brief 格式化错误消息。
     * 
     * @param code 错误码。
     * @param context 错误上下文信息。
     * @return std::string 格式化后的错误消息字符串。
     */
    static std::string format_error_message(ErrorCode code, const std::string& context);
    
    /**
     * @brief 记录错误信息。
     * 
     * @param code 错误码。
     * @param context 错误上下文信息。
     */
    static void log_error(ErrorCode code, const std::string& context);
    
    /**
     * @brief 获取错误码的人类可读描述。
     * 
     * @param code 错误码。
     * @return std::string 错误描述字符串。
     */
    static std::string get_error_description(ErrorCode code);
    
    /**
     * @brief 检查错误码是否表示成功。
     * 
     * @param code 要检查的错误码。
     * @return bool 如果是 SUCCESS 则返回true，否则返回false。
     */
    static bool is_success(ErrorCode code);
    
    /**
     * @brief 检查错误码是否表示错误。
     * 
     * @param code 要检查的错误码。
     * @return bool 如果是错误状态返回true，否则返回false。
     */
    static bool is_error(ErrorCode code);
    
    /**
     * @brief 检查操作是否成功，如果不成功则记录错误并返回false。
     * 
     * @param result 操作的布尔结果。
     * @param error_code 要记录的错误码。
     * @param context 错误的上下文信息。
     * @return bool 如果操作成功则返回true，否则返回false。
     */
    static bool check_and_log(bool result, ErrorCode error_code, const std::string& context);
};