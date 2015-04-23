#ifndef FS_VFS_H
#define FS_VFS_H

#include <typedef.h>
#include <lock.h>

#define VFS_TYPE(f)         ((f) & 0xF)
#define VFS_TYPE_FILE       0x00
#define VFS_TYPE_DIRECTORY  0x01
#define VFS_TYPE_SYMLINK    0x02
#define VFS_TYPE_DEVICE     0x03

#define VFS_FLAG_MOUNTPOINT 0x10

#define VFS_FLAG_READONLY   0x0100

struct vfs_node;
struct vfs_dirent;

typedef uint32 (*vfs_read_function)(struct vfs_node* node, uint32 offset, uint32 length, uint8* buffer);
typedef uint32 (*vfs_write_function)(struct vfs_node* node, uint32 offset, uint32 length, const uint8* buffer);

typedef bool (*vfs_iter_function)(struct vfs_node* node, uint32 offset, struct vfs_dirent* dirent);
typedef struct vfs_node* (*vfs_find_function)(struct vfs_node* node, const char* name);

typedef struct vfs_node
{
    spinlock lock;
    
    uint32 inode_no;
    uint32 flags;
    
    uint32 size;
    
    vfs_read_function read;
    vfs_write_function write;
    
    vfs_iter_function iter;
    vfs_find_function find;
    
    union
    {
        struct vfs_node* mounted_node;
    };
} vfs_node;

typedef struct vfs_dirent
{
    const char* name;
    vfs_node* node;
} vfs_dirent;

#endif
