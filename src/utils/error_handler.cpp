// ==============================================================================
// @file   error_handler.cpp
// @brief  标准化错误处理工具类的实现
// ==============================================================================

#include "error_handler.h"
#include <iostream>
#include <sstream>

/**
 * @brief 格式化错误消息。
 * 
 * @param code 错误码。
 * @param context 错误的上下文信息。
 * @return std::string 格式化后的错误消息字符串。
 */
std::string ErrorHandler::format_error_message(ErrorCode code, const std::string& context) {
    std::ostringstream oss;
    oss << "Error [" << code << "]: " << get_error_description(code);
    
    if (!context.empty()) {
        oss << " (Context: " << context << ")";
    }
    
    return oss.str();
}

/**
 * @brief 将错误信息记录到标准错误流。
 * 
 * @param code 错误码。
 * @param context 错误的上下文信息。
 */
void ErrorHandler::log_error(ErrorCode code, const std::string& context) {
    std::cerr << format_error_message(code, context) << std::endl;
}

/**
 * @brief 获取错误码的人类可读描述。
 * 
 * @param code 错误码。
 * @return std::string 错误描述的英文字符串。
 */
std::string ErrorHandler::get_error_description(ErrorCode code) {
    switch (code) {
        case SUCCESS:                     return "Operation successful";
        case ERROR_DISK_NOT_FOUND:        return "Disk file not found";
        case ERROR_DISK_ALREADY_EXISTS:   return "Disk file already exists";
        case ERROR_INVALID_BLOCK:         return "Invalid block number";
        case ERROR_NO_FREE_BLOCKS:        return "No free blocks available";
        case ERROR_NO_FREE_INODES:        return "No free inodes available";
        case ERROR_FILE_NOT_FOUND:        return "File not found";
        case ERROR_FILE_ALREADY_EXISTS:   return "File already exists";
        case ERROR_INVALID_PATH:          return "Invalid path";
        case ERROR_PERMISSION_DENIED:     return "Permission denied";
        case ERROR_DISK_FULL:             return "Disk full";
        case ERROR_IO_ERROR:              return "I/O error";
        case ERROR_INVALID_INODE:         return "Invalid inode";
        case ERROR_DIRECTORY_NOT_EMPTY:   return "Directory not empty";
        case ERROR_NOT_A_DIRECTORY:       return "Not a directory";
        case ERROR_IS_A_DIRECTORY:        return "Is a directory";
        case ERROR_INVALID_FILE_DESCRIPTOR: return "Invalid file descriptor";
        case ERROR_FILE_ALREADY_OPEN:     return "File already open";
        case ERROR_FILE_NOT_OPEN:         return "File not open";
        case ERROR_INVALID_ARGUMENT:      return "Invalid argument";
        case ERROR_OUT_OF_MEMORY:         return "Out of memory";
        case ERROR_BUFFER_OVERFLOW:       return "Buffer overflow";
        case ERROR_UNKNOWN_COMMAND:       return "Unknown command";
        case ERROR_INVALID_SYNTAX:        return "Invalid syntax";
        case ERROR_MOUNT_FAILED:          return "Mount failed";
        case ERROR_UNMOUNT_FAILED:        return "Unmount failed";
        case ERROR_FORMAT_FAILED:         return "Format failed";
        case ERROR_ALREADY_MOUNTED:       return "Already mounted";
        case ERROR_NOT_MOUNTED:           return "Not mounted";
        default:                          return "Unknown error";
    }
}

/**
 * @brief 检查错误码是否表示成功。
 * 
 * @param code 要检查的错误码。
 * @return bool 如果是 SUCCESS 则返回true，否则返回false。
 */
bool ErrorHandler::is_success(ErrorCode code) {
    return code == SUCCESS;
}

/**
 * @brief 检查错误码是否表示错误。
 * 
 * @param code 要检查的错误码。
 * @return bool 如果不是 SUCCESS 则返回true，否则返回false。
 */
bool ErrorHandler::is_error(ErrorCode code) {
    return code != SUCCESS;
}

/**
 * @brief 检查操作是否成功，如果不成功则记录错误并返回false。
 * 
 * @param result 操作的布尔结果。
 * @param error_code 要记录的错误码。
 * @param context 错误的上下文信息。
 * @return bool 如果操作成功则返回true，否则返回false。
 */
bool ErrorHandler::check_and_log(bool result, ErrorCode error_code, const std::string& context) {
    if (!result) {
        log_error(error_code, context);
    }
    return result;
}