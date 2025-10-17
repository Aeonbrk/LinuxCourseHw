#pragma once

/**
 * @enum ErrorCode
 * @brief 定义了文件系统所有操作可能返回的错误码。
 */
enum ErrorCode {
    SUCCESS = 0,                           ///< 操作成功
    ERROR_DISK_NOT_FOUND = -1,             ///< 磁盘文件未找到
    ERROR_DISK_ALREADY_EXISTS = -2,        ///< 磁盘文件已存在
    ERROR_INVALID_BLOCK = -3,              ///< 无效的块号
    ERROR_NO_FREE_BLOCKS = -4,             ///< 没有空闲块
    ERROR_NO_FREE_INODES = -5,             ///< 没有空闲inode
    ERROR_FILE_NOT_FOUND = -6,             ///< 文件未找到
    ERROR_FILE_ALREADY_EXISTS = -7,        ///< 文件已存在
    ERROR_INVALID_PATH = -8,               ///< 无效的路径
    ERROR_PERMISSION_DENIED = -9,          ///< 权限被拒绝
    ERROR_DISK_FULL = -10,                 ///< 磁盘已满
    ERROR_IO_ERROR = -11,                  ///< 输入输出错误
    ERROR_INVALID_INODE = -12,             ///< 无效的inode
    ERROR_DIRECTORY_NOT_EMPTY = -13,       ///< 目录不为空
    ERROR_NOT_A_DIRECTORY = -14,           ///< 不是目录
    ERROR_IS_A_DIRECTORY = -15,            ///< 是目录
    ERROR_INVALID_FILE_DESCRIPTOR = -16,   ///< 无效的文件描述符
    ERROR_FILE_ALREADY_OPEN = -17,         ///< 文件已打开
    ERROR_FILE_NOT_OPEN = -18,             ///< 文件未打开
    ERROR_INVALID_ARGUMENT = -19,          ///< 无效的参数
    ERROR_OUT_OF_MEMORY = -20,             ///< 内存不足
    ERROR_BUFFER_OVERFLOW = -21,           ///< 缓冲区溢出
    ERROR_UNKNOWN_COMMAND = -22,           ///< 未知命令
    ERROR_INVALID_SYNTAX = -23,            ///< 无效的语法
    ERROR_MOUNT_FAILED = -24,              ///< 挂载失败
    ERROR_UNMOUNT_FAILED = -25,            ///< 卸载失败
    ERROR_FORMAT_FAILED = -26,             ///< 格式化失败
    ERROR_ALREADY_MOUNTED = -27,           ///< 已挂载
    ERROR_NOT_MOUNTED = -28                ///< 未挂载
};

/**
 * @brief 根据错误码获取对应的错误信息字符串。
 * @param error 要转换的错误码。
 * @return const char* 对应的错误信息字符串。
 */
inline const char* get_error_message(ErrorCode error) {
    switch (error) {
        case SUCCESS: return "Success";
        case ERROR_DISK_NOT_FOUND: return "Disk file not found";
        case ERROR_DISK_ALREADY_EXISTS: return "Disk file already exists";
        case ERROR_INVALID_BLOCK: return "Invalid block number";
        case ERROR_NO_FREE_BLOCKS: return "No free blocks available";
        case ERROR_NO_FREE_INODES: return "No free inodes available";
        case ERROR_FILE_NOT_FOUND: return "File not found";
        case ERROR_FILE_ALREADY_EXISTS: return "File already exists";
        case ERROR_INVALID_PATH: return "Invalid path";
        case ERROR_PERMISSION_DENIED: return "Permission denied";
        case ERROR_DISK_FULL: return "Disk full";
        case ERROR_IO_ERROR: return "I/O error";
        case ERROR_INVALID_INODE: return "Invalid inode";
        case ERROR_DIRECTORY_NOT_EMPTY: return "Directory not empty";
        case ERROR_NOT_A_DIRECTORY: return "Not a directory";
        case ERROR_IS_A_DIRECTORY: return "Is a directory";
        case ERROR_INVALID_FILE_DESCRIPTOR: return "Invalid file descriptor";
        case ERROR_FILE_ALREADY_OPEN: return "File already open";
        case ERROR_FILE_NOT_OPEN: return "File not open";
        case ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case ERROR_OUT_OF_MEMORY: return "Out of memory";
        case ERROR_BUFFER_OVERFLOW: return "Buffer overflow";
        case ERROR_UNKNOWN_COMMAND: return "Unknown command";
        case ERROR_INVALID_SYNTAX: return "Invalid syntax";
        case ERROR_MOUNT_FAILED: return "Mount failed";
        case ERROR_UNMOUNT_FAILED: return "Unmount failed";
        case ERROR_FORMAT_FAILED: return "Format failed";
        case ERROR_ALREADY_MOUNTED: return "Already mounted";
        case ERROR_NOT_MOUNTED: return "Not mounted";
        default: return "Unknown error";
    }
}