#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"

#define NEWFS_MAGIC                  /* TODO: Define by yourself */
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */
/******************************************************************************
 * SECTION: macro debug
 *******************************************************************************/
#define SFS_DBG(fmt, ...) do { printf("SFS_DBG: " fmt, ##__VA_ARGS__); } while(0)
/******************************************************************************
 * SECTION: newfs.c
 *******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);
/******************************************************************************
* SECTION: newfs_utils.c
*******************************************************************************/
int 				fs_driver_read(int offset, uint8_t *out_content, int size);
int 				fs_driver_write(int offset, uint8_t *in_content, int size);
int 				fs_mount(struct custom_options options);
int 				fs_umount();
char* 				fs_get_fname(const char* path);
int 				fs_calc_lvl(const char * path);
int 				fs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry);
int 				fs_drop_dentry(struct newfs_inode * inode, struct newfs_dentry * dentry);
struct newfs_inode* fs_alloc_inode(struct newfs_dentry * dentry);
int 				fs_sync_inode(struct newfs_inode * inode);
int 				fs_drop_inode(struct newfs_inode * inode);
struct newfs_inode* fs_read_inode(struct newfs_dentry * dentry, int ino);
struct newfs_dentry* fs_get_dentry(struct newfs_inode * inode, int dir);
struct newfs_dentry* fs_lookup(const char * path, boolean* is_find, boolean* is_root);
int 				fs_free_data(int data_num);
int 				fs_alloc_data();
void 				fs_dump_map();
#endif  /* _newfs_H_ */