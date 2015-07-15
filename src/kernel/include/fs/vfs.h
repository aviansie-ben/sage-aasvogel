#ifndef FS_VFS_H
#define FS_VFS_H

#include <typedef.h>
#include <lock/spinlock.h>
#include <core/bootparam.h>

#define VFS_TYPE(f)         ((f) & 0xF)
#define VFS_TYPE_FILE       0x00
#define VFS_TYPE_DIRECTORY  0x01
#define VFS_TYPE_SYMLINK    0x02
#define VFS_TYPE_DEVICE     0x03

#define VFS_FLAG_MOUNTPOINT 0x10

#define VFS_FLAG_READONLY   0x0100

struct fs_type;
struct fs_device;
struct fs_info;

struct vfs_node;
struct vfs_dirent;

typedef uint32 (*fs_type_try_read)(struct fs_type* type, struct fs_device* dev, struct fs_info* info);

typedef uint32 (*fs_device_read)(struct fs_device* dev, uint32 offset, uint32 length, uint8* buffer);
typedef uint32 (*fs_device_write)(struct fs_device* dev, uint32 offset, uint32 length, const uint8* buffer);

typedef struct fs_type
{
    const char* name;
    
    fs_type_try_read try_read;
    
    struct fs_type* next;
} fs_type;

typedef struct fs_device
{
    spinlock lock;
    
    fs_device_read read;
    fs_device_write write;
} fs_device;

typedef struct fs_info
{
    spinlock lock;
    
    fs_type* type;
    fs_device* device;
    
    struct vfs_node* root_node;
} fs_info;

typedef uint32 (*vfs_read_function)(struct vfs_node* node, uint32 offset, uint32 length, uint8* buffer);
typedef uint32 (*vfs_write_function)(struct vfs_node* node, uint32 offset, uint32 length, const uint8* buffer);

typedef bool (*vfs_iter_function)(struct vfs_node* node, uint32 offset, struct vfs_dirent* dirent);
typedef struct vfs_node* (*vfs_find_function)(struct vfs_node* node, const char* name);

typedef struct vfs_node
{
    spinlock lock;
    
    fs_info* fs;
    
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

extern spinlock fs_type_lock;
extern vfs_node vfs_root;

extern void vfs_init(const boot_param* param);
extern void vfs_register_fs(fs_type* type);
extern void vfs_unregister_fs(fs_type* type);

#endif
