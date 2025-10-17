#pragma once
#include <string>
#include <vector>
#include <ctime>

// ==================== 基本常量定义 ====================

const int BLOCK_SIZE = 4096;  ///< 磁盘块大小（字节）
const int BITS_PER_BLOCK = BLOCK_SIZE * 8; ///< 每个块的位数
const int DISK_SIZE = 100 * 1024 * 1024;  ///< 磁盘总大小（100MB）
const int MAX_FILENAME_LENGTH = 256;  ///< 文件名最大长度
const int MAX_PATH_LENGTH = 1024;  ///< 路径最大长度
const int DIRECT_BLOCKS_COUNT = 10;  ///< 直接块指针数量
const int MAGIC_NUMBER = 0x4D494E44;  ///< 文件系统魔数，用于识别文件系统类型 ("DMIN")

// ==================== 文件类型和权限定义 ====================

const int FILE_TYPE_REGULAR = 0x8000;  ///< 普通文件类型
const int FILE_TYPE_DIRECTORY = 0x4000;  ///< 目录文件类型
const int FILE_PERMISSION_READ = 0x400;  ///< 读权限
const int FILE_PERMISSION_WRITE = 0x200;  ///< 写权限
const int FILE_PERMISSION_EXECUTE = 0x100;  ///< 执行权限

// ==================== 文件打开模式定义 ====================

const int OPEN_MODE_READ = 0x01;  ///< 只读模式
const int OPEN_MODE_WRITE = 0x02;  ///< 写模式
const int OPEN_MODE_CREATE = 0x04;  ///< 创建模式（如果文件不存在则创建）
const int OPEN_MODE_APPEND = 0x08;  ///< 追加模式

// ==================== 数据结构前向声明 ====================

struct Superblock;
struct Inode;
struct DirectoryEntry;
struct DiskLayout;

// ==================== 磁盘布局结构 ====================

/**
 * @struct DiskLayout
 * @brief 定义了文件系统在磁盘上的完整布局。
 */
struct DiskLayout {
  int superblock_start;  ///< 超级块起始位置（块号）
  int superblock_blocks;  ///< 超级块占用块数
  int inode_bitmap_start;  ///< inode位图起始位置（块号）
  int inode_bitmap_blocks;  ///< inode位图占用块数
  int data_bitmap_start;  ///< 数据块位图起始位置（块号）
  int data_bitmap_blocks;  ///< 数据块位图占用块数
  int inode_table_start;  ///< inode表起始位置（块号）
  int inode_table_blocks;  ///< inode表占用块数
  int data_blocks_start;  ///< 数据块起始位置（块号）
  int data_blocks_count;  ///< 数据块数量
};

// ==================== 超级块结构 ====================

/**
 * @struct Superblock
 * @brief 存储文件系统的全局元信息。
 */
struct Superblock {
  int magic_number;  ///< 魔数，用于文件系统识别
  int total_blocks;  ///< 磁盘总块数
  int free_blocks;  ///< 空闲块数
  int total_inodes;  ///< 总inode数
  int free_inodes;  ///< 空闲inode数
  int block_size;  ///< 块大小（字节）
  int inode_table_start;  ///< inode表起始位置（块号）
  int data_blocks_start;  ///< 数据块起始位置（块号）
  int inode_bitmap_start;  ///< inode位图起始位置（块号）
  int data_bitmap_start;  ///< 数据块位图起始位置（块号）
  time_t mount_time;  ///< 文件系统挂载时间
  time_t write_time;  ///< 最后写入时间
};

// ==================== Inode结构 ====================

/**
 * @struct Inode
 * @brief 存储文件或目录的元数据信息（索引节点）。
 */
struct Inode {
  int mode;  ///< 文件类型和权限
  int owner_id;  ///< 所有者ID
  int group_id;  ///< 组ID
  int size;  ///< 文件大小（字节）
  time_t access_time;  ///< 最后访问时间
  time_t modification_time;  ///< 最后修改时间
  time_t creation_time;  ///< 创建时间
  int link_count;  ///< 硬链接计数
  int direct_blocks[DIRECT_BLOCKS_COUNT];  ///< 直接块指针数组
  int indirect_block;  ///< 一级间接块指针
  int double_indirect_block;  ///< 二级间接块指针
};

// ==================== 目录项结构 ====================

/**
 * @struct DirectoryEntry
 * @brief 存储目录中的一个文件或子目录的条目。
 */
struct DirectoryEntry {
  int inode_number;  ///< 对应的inode编号
  char name[MAX_FILENAME_LENGTH];  ///< 文件名（以null结尾的字符串）
  int name_length;  ///< 文件名实际长度
};

// ==================== 文件描述符结构 ====================

/**
 * @struct FileDescriptor
 * @brief 用于跟踪一个已打开文件的状态。
 */
struct FileDescriptor {
  int inode_num;  ///< 关联的inode编号
  int mode;  ///< 打开模式（读/写等）
  int position;  ///< 当前读写位置（字节偏移）
  bool open;  ///< 文件是否处于打开状态
};

// ==================== 命令结构 ====================

/**
 * @struct Command
 * @brief 用于存储从CLI解析后的命令及其参数。
 */
struct Command {
  std::string name;  ///< 命令名称
  std::vector<std::string> args;  ///< 命令参数列表
};