#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128
/******************************************************************************
* SECTION: Type def
*******************************************************************************/
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum file_type
{
    FS_FILE, // 普通文件
    FS_DIR   // 目录文件
} FILE_TYPE;

/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define SFS_MAGIC_NUM           0x52415453  
#define SFS_SUPER_OFS           0
#define SFS_ROOT_INO            0

#define SFS_ERROR_NONE          0
#define SFS_ERROR_ACCESS        EACCES
#define SFS_ERROR_SEEK          ESPIPE     
#define SFS_ERROR_ISDIR         EISDIR
#define SFS_ERROR_NOSPACE       ENOSPC
#define SFS_ERROR_EXISTS        EEXIST
#define SFS_ERROR_NOTFOUND      ENOENT
#define SFS_ERROR_UNSUPPORTED   ENXIO
#define SFS_ERROR_IO            EIO     /* Error Input/Output */
#define SFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define SFS_MAX_FILE_NAME       128
#define SFS_INODE_PER_FILE      1
#define SFS_DATA_PER_FILE       6
#define SFS_DEFAULT_PERM        0777

#define SFS_IOC_MAGIC           'S'
#define SFS_IOC_SEEK            _IO(SFS_IOC_MAGIC, 0)

#define SFS_FLAG_BUF_DIRTY      0x1
#define SFS_FLAG_BUF_OCCUPY     0x2
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define SFS_IO_SZ()                     (newfs_super.sz_io)
#define SFS_BLOCK_SZ()                   (newfs_super.sz_block)
#define SFS_DISK_SZ()                   (newfs_super.sz_disk)
#define SFS_DRIVER()                    (newfs_super.driver_fd)

#define SFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define SFS_ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)

#define SFS_BLKS_SZ(blks)               (blks * SFS_BLOCK_SZ())
#define SFS_ASSIGN_FNAME(pnewfs_dentry, _fname) memcpy(pnewfs_dentry->fname, _fname, strlen(_fname))
#define SFS_INO_OFS(ino)                (newfs_super.inode_offset + ino * SFS_BLOCK_SZ())
#define SFS_DATA_OFS(ino)               (newfs_super.data_offset + ino * SFS_BLOCK_SZ())

#define SFS_IS_DIR(pinode)              (pinode->dentry->ftype == FS_DIR)
#define SFS_IS_FILE(pinode)              (pinode->dentry->ftype == FS_FILE)

struct custom_options {
	const char*        device;
};
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct newfs_super {
    // uint32_t    magic_num;  // 幻数
    int         driver_fd;
    /* TODO: Define yourself */
    int         max_ino;

    int         sz_io;
    int         sz_block;
    int         sz_disk;

    uint8_t     *map_inode; // inode位图
    uint8_t     *map_data;  // data位图
    int         map_inode_blks; // inode 位图占用的块数
    int         map_inode_offset; // inode 位图在磁盘上的偏移

    int         map_data_blks; // data 位图占用的块数
    int         map_data_offset; // data 位图在磁盘上的偏移

    struct newfs_dentry *root_dentry; // 根目录dentry

    int data_offset;
    int inode_offset;

    boolean is_mounted;

    int         sz_usage;
};

struct newfs_inode {
    /* TODO: Define yourself */
    int ino;                // 在inode位图中的下标
    int size;               // 文件已占用空间
    int dir_cnt;            // 目录项数量
    struct newfs_dentry *dentry;  // 指向该inode的dentry
    struct newfs_dentry *dentrys; // 所有目录项
    int block_pointer[6];   // 数据块指针
    uint8_t* data[6]; 
    
};

struct newfs_dentry {
    /* TODO: Define yourself */
    char                fname[MAX_NAME_LEN];
    uint32_t            ino;
    struct newfs_inode*   inode; 
    struct newfs_dentry*  parent;                        /* 父亲Inode的dentry */
    struct newfs_dentry*  brother;                       /* 兄弟 */
    FILE_TYPE       ftype; // 指向的 ino 文件类型
    int             valid; // 该目录项是否有效
};

static inline struct newfs_dentry* new_dentry(char * fname, FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    SFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode = NULL;
    dentry->parent = NULL;
    dentry->brother = NULL;
    return dentry;                                    
}
/******************************************************************************
 * SECTION: FS Specific Structure - Disk structure
 *******************************************************************************/
struct newfs_super_d
{ 
    uint32_t    magic_num;

    int         max_ino; // 最多支持的文件数

    int         map_inode_blks; // inode 位图占用的块数
    int         map_inode_offset; // inode 位图在磁盘上的偏移

    int         map_data_blks; // data 位图占用的块数
    int         map_data_offset; // data 位图在磁盘上的偏移

    int         inode_offset;
    int         data_offset;

    int         sz_usage;
};

struct newfs_inode_d
{
    uint32_t    ino;                // 在 inode 位图中的下标
    int         size;               // 文件已占用空间
    // int         link;               // 链接数
    FILE_TYPE   ftype;              // 文件类型（目录类型、普通文件类型）
    int         dir_cnt;            // 如果是目录类型文件，下面有几个目录项
    int         block_pointer[6];   // 数据块指针（可固定分配）
};

struct newfs_dentry_d
{
    char               fname[SFS_MAX_FILE_NAME];
    FILE_TYPE          ftype;
    int                ino;                           /* 指向的ino号 */
};  

#endif /* _TYPES_H_ */