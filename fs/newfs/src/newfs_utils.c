#include "../include/newfs.h"
extern struct newfs_super newfs_super;
extern struct custom_options newfs_options;

/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int fs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = SFS_ROUND_DOWN(offset, SFS_BLOCK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = SFS_ROUND_UP((size + bias), SFS_BLOCK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_read(SFS_DRIVER(), (char*)cur, SFS_IO_SZ());
        cur          += SFS_IO_SZ();
        size_aligned -= SFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return SFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int fs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = SFS_ROUND_DOWN(offset, SFS_BLOCK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = SFS_ROUND_UP((size + bias), SFS_BLOCK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    fs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(SFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(SFS_DRIVER(), cur, SFS_IO_SZ());
        ddriver_write(SFS_DRIVER(), (char*)cur, SFS_IO_SZ());
        cur          += SFS_IO_SZ();
        size_aligned -= SFS_IO_SZ();   
    }

    free(temp_content);
    return SFS_ERROR_NONE;
}

/**
 * @brief 挂载sfs, Layout 如下
 * 
 * Layout
 * | Super(1) | Inode Map(1) | DATA Map(4) | DATA(*) |
 * 
 * IO_SZ * 2 = BLOCK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int fs_mount(struct custom_options options){
    int                 ret = SFS_ERROR_NONE;
    int                 driver_fd;
    struct newfs_super_d  newfs_super_d; 
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int                 inode_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    
    int                 super_blks;
    boolean             is_init = FALSE;

    newfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open((char*)options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    newfs_super.driver_fd = driver_fd;
    ddriver_ioctl(SFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &newfs_super.sz_disk);
    ddriver_ioctl(SFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &newfs_super.sz_io);
    newfs_super.sz_block = 2* newfs_super.sz_io;
    SFS_DBG("disk size: %d\n", newfs_super.sz_disk);
    SFS_DBG("io size: %d\n", newfs_super.sz_io);
    
    root_dentry = new_dentry("/", FS_DIR);

    if (fs_driver_read(SFS_SUPER_OFS, (uint8_t *)(&newfs_super_d), 
                        sizeof(struct newfs_super_d)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }   
                                                      /* 读取super */
    if (newfs_super_d.magic_num != SFS_MAGIC_NUM) {     /* 幻数无 */
                                                      /* 估算各部分大小 */
        super_blks = SFS_ROUND_UP(sizeof(struct newfs_super_d), SFS_BLOCK_SZ()) / SFS_BLOCK_SZ();

        inode_num  =  SFS_DISK_SZ() / ((SFS_DATA_PER_FILE + SFS_INODE_PER_FILE) * SFS_BLOCK_SZ());

        map_inode_blks = SFS_ROUND_UP(SFS_ROUND_UP(inode_num, UINT32_BITS), SFS_BLOCK_SZ()) 
                         / SFS_BLOCK_SZ();

        map_data_blks = SFS_ROUND_UP(SFS_DISK_SZ() / SFS_BLOCK_SZ(),SFS_BLOCK_SZ())/SFS_BLOCK_SZ();
        
        /* 布局layout */
        newfs_super.max_ino = (inode_num - super_blks - map_inode_blks - map_data_blks); 
        newfs_super_d.map_inode_offset = SFS_SUPER_OFS + SFS_BLKS_SZ(super_blks);
        newfs_super_d.map_data_offset =  newfs_super_d.map_inode_offset + SFS_BLKS_SZ(map_inode_blks);
        newfs_super_d.inode_offset = newfs_super_d.map_data_offset + SFS_BLKS_SZ(map_data_blks);
        newfs_super_d.data_offset = newfs_super_d.map_inode_offset + SFS_BLKS_SZ(inode_num);
        newfs_super_d.map_inode_blks  = map_inode_blks;
        newfs_super_d.map_data_blks = map_data_blks;
        SFS_DBG("inode map blocks: %d\n", map_inode_blks);
        SFS_DBG("data map blocks: %d\n", map_data_blks);
        is_init = TRUE;
    }
    
    newfs_super.map_inode = (uint8_t *)malloc(SFS_BLKS_SZ(newfs_super_d.map_inode_blks));
    newfs_super.map_data = (uint8_t *)malloc(SFS_BLKS_SZ(newfs_super_d.map_data_blks));
    newfs_super.map_inode_blks = newfs_super_d.map_inode_blks;
    newfs_super.map_inode_offset = newfs_super_d.map_inode_offset;
    newfs_super.map_data_blks = newfs_super_d.map_data_blks;
    newfs_super.map_data_offset = newfs_super_d.map_data_offset;
    newfs_super.data_offset = newfs_super_d.data_offset;
    newfs_super.inode_offset = newfs_super_d.inode_offset;
    newfs_super.sz_usage = newfs_super_d.sz_usage;

    if (fs_driver_read(newfs_super_d.map_inode_offset, (uint8_t *)(newfs_super.map_inode), 
                        SFS_BLKS_SZ(newfs_super_d.map_inode_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }
    if (fs_driver_read(newfs_super_d.map_data_offset, (uint8_t *)(newfs_super.map_data), 
                        SFS_BLKS_SZ(newfs_super_d.map_data_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = fs_alloc_inode(root_dentry);
        fs_sync_inode(root_inode);
    }
    
    root_inode            = fs_read_inode(root_dentry, SFS_ROOT_INO);
    root_dentry->inode      = root_inode;
    newfs_super.root_dentry = root_dentry;
    newfs_super.is_mounted  = TRUE;

    fs_dump_map();
    return ret;
}
/**
 * @brief 
 * 
 * @return int 
 */
int fs_umount() {
    struct newfs_super_d  newfs_super_d; 

    if (!newfs_super.is_mounted) {
        return SFS_ERROR_NONE;
    }

    fs_sync_inode(newfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    newfs_super_d.magic_num           = SFS_MAGIC_NUM;
    newfs_super_d.map_inode_blks      = newfs_super.map_inode_blks;
    newfs_super_d.map_inode_offset    = newfs_super.map_inode_offset;
    newfs_super_d.map_data_blks      = newfs_super.map_data_blks;
    newfs_super_d.map_data_offset    = newfs_super.map_data_offset;
    newfs_super_d.data_offset         = newfs_super.data_offset;
    newfs_super_d.inode_offset         = newfs_super.inode_offset;
    newfs_super_d.max_ino             = newfs_super.max_ino;
    newfs_super_d.sz_usage            = newfs_super.sz_usage;

    if (fs_driver_write(SFS_SUPER_OFS, (uint8_t *)&newfs_super_d, 
                     sizeof(struct newfs_super_d)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }

    if (fs_driver_write(newfs_super_d.map_inode_offset, (uint8_t *)(newfs_super.map_inode), 
                         SFS_BLKS_SZ(newfs_super_d.map_inode_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }
    if (fs_driver_write(newfs_super_d.map_data_offset, (uint8_t *)(newfs_super.map_data), 
                         SFS_BLKS_SZ(newfs_super_d.map_data_blks)) != SFS_ERROR_NONE) {
        return -SFS_ERROR_IO;
    }

    free(newfs_super.map_inode);
    free(newfs_super.map_data);
    ddriver_close(SFS_DRIVER());

    return SFS_ERROR_NONE;
}


/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* fs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int fs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int fs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
        inode->block_pointer[0] = fs_alloc_data();
        inode->data[0] = (uint8_t *)malloc(SFS_BLOCK_SZ());
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int fs_drop_dentry(struct newfs_inode * inode, struct newfs_dentry * dentry) {
    boolean is_find = FALSE;
    struct newfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -SFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return sfs_inode
 */
struct newfs_inode* fs_alloc_inode(struct newfs_dentry * dentry) {
    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = FALSE;

    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((newfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                newfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == newfs_super.max_ino)
        // return -SFS_ERROR_NOSPACE;
        return NULL;

    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    // dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    if (SFS_IS_FILE(inode)) {
        for (int i=0;i<SFS_DATA_PER_FILE;i++) {
            inode->block_pointer[i] = fs_alloc_data();
            inode->data[i] = (uint8_t*)malloc(SFS_BLOCK_SZ());
        }
    }
    // else if (SFS_IS_DIR(inode)) {
    //     inode->block_pointer[0] = fs_alloc_data();
    //     inode->data[0] = (uint8_t *)malloc(SFS_BLOCK_SZ());
    // }

    return inode;
}
/**
 * @brief 分配一个数据块，占用位图
 * @return data block对应的编号
 */
int fs_alloc_data() {
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int data_cursor  = 0;
    boolean is_find_free_entry = FALSE;

    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_data_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((newfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                newfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            data_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || (byte_cursor == SFS_BLKS_SZ(newfs_super.map_data_blks) && (bit_cursor == UINT8_BITS)))
        return -SFS_ERROR_NOSPACE;
    
    return data_cursor;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int fs_sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    // memcpy(inode_d.target_path, inode->target_path, SFS_MAX_FILE_NAME);
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int offset;

    if (fs_driver_write(SFS_INO_OFS(ino), (uint8_t *)&inode_d,
                        sizeof(struct newfs_inode_d)) != SFS_ERROR_NONE)
    {
        SFS_DBG("[%s] io error\n", __func__);
        return -SFS_ERROR_IO;
    }
                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    if (SFS_IS_DIR(inode)) {                          
        dentry_cursor = inode->dentrys;
        offset        = SFS_DATA_OFS(inode->block_pointer[0]);
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.fname, dentry_cursor->fname, SFS_MAX_FILE_NAME);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (fs_driver_write(offset, (uint8_t *)&dentry_d, 
                                 sizeof(struct newfs_dentry_d)) != SFS_ERROR_NONE) {
                SFS_DBG("[%s] io error\n", __func__);
                return -SFS_ERROR_IO;                     
            }
            
            if (dentry_cursor->inode != NULL) {
                fs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct newfs_dentry_d);
            if(offset > SFS_BLOCK_SZ())
                SFS_DBG("[%s] io error\n", __func__);
        }
    }
    else if (SFS_IS_FILE(inode)) {
        for (int i=0;i<SFS_DATA_PER_FILE;i++) {
            if (fs_driver_write(SFS_DATA_OFS(inode->block_pointer[i]), inode->data[i], SFS_BLKS_SZ(SFS_DATA_PER_FILE)) !=SFS_ERROR_NONE) {
                SFS_DBG("[%s] io error\n", __func__);
                return -SFS_ERROR_IO;
            }
        }
        
    }
    return SFS_ERROR_NONE;
}
/**
 * @brief 删除内存中的一个inode， 暂时不释放
 * Case 1: Reg File
 * 
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 * 
 *  1) Step 1. Erase Bitmap     
 *  2) Step 2. Free Inode                      (Function of sfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 * 
 *   Recursive
 * @param inode 
 * @return int 
 */
int fs_drop_inode(struct newfs_inode * inode) {
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry*  dentry_to_free;
    struct newfs_inode*   inode_cursor;

    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;

    if (inode == newfs_super.root_dentry->inode) {
        return SFS_ERROR_INVAL;
    }

    if (SFS_IS_DIR(inode)) {
        dentry_cursor = inode->dentrys;
                                                      /* 递归向下drop */
        while (dentry_cursor)
        {   
            inode_cursor = dentry_cursor->inode;
            fs_drop_inode(inode_cursor);
            fs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }
    }
    else if (SFS_IS_FILE(inode)) {
        for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     newfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
        for(int i=0;i<SFS_DATA_PER_FILE;i++) {
            if (inode->data[i])
                free(inode->data[i]);
            if (inode->block_pointer[i]) {
                fs_free_data(inode->block_pointer[i]);
            }
        }
        
        free(inode);
    }
    return SFS_ERROR_NONE;
}
/**
 * @brief 在数据位图中修改被释放的数据块的标识
 * @param data_num 数据块的编号 
 * @return int 
 */
int fs_free_data(int data_num) {
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;
    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_data_blks); 
            byte_cursor++)                            /* 调整datamap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == data_num) {
                     newfs_super.map_data[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
    return SFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct sfs_inode* 
 */
struct newfs_inode* fs_read_inode(struct newfs_dentry * dentry, int ino) {
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry;
    struct newfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    if (fs_driver_read(SFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != SFS_ERROR_NONE) {
        SFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    // memcpy(inode->target_path, inode_d.target_path, SFS_MAX_FILE_NAME);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    if (SFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            if (fs_driver_read(SFS_DATA_OFS(inode_d.block_pointer[0]) + i * sizeof(struct newfs_dentry_d),
                               (uint8_t *)&dentry_d,
                               sizeof(struct newfs_dentry_d)) != SFS_ERROR_NONE)
            {
                SFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            fs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (SFS_IS_FILE(inode)) {
        for(int i=0;i<SFS_DATA_PER_FILE;i++) {
            inode->data[i] = (uint8_t *)malloc(SFS_BLOCK_SZ());
            if (fs_driver_read(SFS_DATA_OFS(inode_d.block_pointer[0]), (uint8_t *)inode->data, 
                                SFS_BLOCK_SZ()) != SFS_ERROR_NONE) {
                SFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
        }
    }
    return inode;
}
/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct sfs_dentry* 
 */
struct newfs_dentry* fs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct sfs_inode* 
 */
struct newfs_dentry* fs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry* dentry_cursor = newfs_super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode*  inode; 
    int   total_lvl = fs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = newfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            fs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (SFS_IS_FILE(inode) && lvl < total_lvl) {
            SFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (SFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                SFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = fs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

void fs_dump_map()
{
    int byte_cursor = 0;
    int bit_cursor = 0;

    printf("inode map:\n");

    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_inode_blks);
         byte_cursor += 4)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\n");
    }

    printf("data map:\n");

    for (byte_cursor = 0; byte_cursor < SFS_BLKS_SZ(newfs_super.map_data_blks);
         byte_cursor += 4)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_data[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_data[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            printf("%d ", (newfs_super.map_data[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);
        }
        printf("\n");
    }
}