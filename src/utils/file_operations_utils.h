#pragma once
#include <string>
#include <vector>

#include "../core/disk_simulator.h"
#include "../core/inode_manager.h"
#include "../core/path_manager.h"
#include "common.h"
#include "error_codes.h"
#include "error_handler.h"

/**
 * @class FileOperationsUtils
 * @brief 提供通用文件操作的静态工具类。
 * 
 * 此类提供静态方法，用于处理文件系统中常见的公共操作，
 * 以减少代码重复并在FileManager和DirectoryManager之间共享逻辑。
 */
class FileOperationsUtils {
public:
    /**
     * @brief 从数据块读取数据到缓冲区。
     * @param disk DiskSimulator的引用。
     * @param blocks 数据块列表。
     * @param offset 偏移量。
     * @param buffer 输出缓冲区。
     * @param size 要读取的大小。
     * @return bool 读取成功返回true，否则返回false。
     */
    static bool read_data_from_blocks(DiskSimulator& disk, 
                                      const std::vector<int>& blocks, 
                                      int offset, 
                                      char* buffer, 
                                      int size);
                                      
    /**
     * @brief 将缓冲区数据写入到数据块。
     * @param disk DiskSimulator的引用。
     * @param blocks 数据块列表。
     * @param offset 偏移量。
     * @param buffer 输入缓冲区。
     * @param size 要写入的大小。
     * @return bool 写入成功返回true，否则返回false。
     */
    static bool write_data_to_blocks(DiskSimulator& disk, 
                                     const std::vector<int>& blocks, 
                                     int offset, 
                                     const char* buffer, 
                                     int size);
                                     
    /**
     * @brief 初始化一个新的inode结构体。
     * @param[out] inode 要被初始化的inode对象。
     * @param mode 文件类型和权限
     * @param link_count 硬链接计数，默认为1
     */
    static void initialize_new_inode(Inode& inode, int mode = FILE_TYPE_REGULAR | 
                                           FILE_PERMISSION_READ | 
                                           FILE_PERMISSION_WRITE, 
                                           int link_count = 1);
};