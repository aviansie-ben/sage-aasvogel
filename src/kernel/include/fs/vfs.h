/**
 * \file
 * TODO: Add file description
 */

#ifndef FS_VFS_H
#define FS_VFS_H

#include <typedef.h>
#include <refcount.h>
#include <lock/mutex.h>
#include <memory/pool.h>
#include <core/bootparam.h>

#define VFS_SEPARATOR_CHAR '/'
#define VFS_SEPARATOR      "/"

#define VFS_NAME_LENGTH 255
#define VFS_PATH_LENGTH 1023
#define VFS_SYMLINK_DEPTH 20

#define VFS_TYPE(f)          ((f) & 0xf)
#define VFS_TYPE_FILE        0x00
#define VFS_TYPE_DIRECTORY   0x01
#define VFS_TYPE_SYMLINK     0x02

#define VFS_FLAG_MOUNTPOINT  0x10

struct vfs_fs_type;
struct vfs_device;

struct vfs_node;
struct vfs_dirent;

typedef __warn_unused_result uint32 (*vfs_device_read_function)(struct vfs_device* dev, uint64 sector, size_t num_sectors, uint8* buffer);
typedef __warn_unused_result uint32 (*vfs_device_write_function)(struct vfs_device* dev, uint64 sector, size_t num_sectors, const uint8* buffer);

typedef __warn_unused_result uint32 (*vfs_fs_try_read_function)(struct vfs_fs_type* type, struct vfs_device* dev);
typedef uint32 (*vfs_fs_destroy_function)(struct vfs_device* dev, bool force);

typedef __warn_unused_result uint32 (*vfs_fs_load_function)(struct vfs_device* dev, uint64 inode_no, struct vfs_node** node);
typedef void (*vfs_fs_unload_function)(struct vfs_node* node);

typedef __warn_unused_result uint32 (*vfs_fs_create_file_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);
typedef __warn_unused_result uint32 (*vfs_fs_create_dir_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);
typedef __warn_unused_result uint32 (*vfs_fs_create_symlink_function)(struct vfs_node* parent, const char* name, const char* target, struct vfs_node** child);

typedef __warn_unused_result uint32 (*vfs_fs_read_function)(struct vfs_node* node, uint64 offset, size_t length, uint8* buffer);
typedef __warn_unused_result uint32 (*vfs_fs_write_function)(struct vfs_node* node, uint64 offset, size_t length, const uint8* buffer);
typedef __warn_unused_result uint32 (*vfs_fs_read_symlink_function)(struct vfs_node* node, char* target);

typedef __warn_unused_result uint32 (*vfs_fs_iter_function)(struct vfs_node* parent, uint32 offset, struct vfs_dirent* dirent);
typedef __warn_unused_result uint32 (*vfs_fs_find_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);

typedef __warn_unused_result uint32 (*vfs_fs_move_function)(struct vfs_node* old_parent, const char* old_name, struct vfs_node* new_parent, const char* new_name);
typedef __warn_unused_result uint32 (*vfs_fs_link_function)(struct vfs_node* parent, const char* name, struct vfs_node* child);
typedef __warn_unused_result uint32 (*vfs_fs_unlink_function)(struct vfs_node* parent, const char* name);

typedef struct
{
    vfs_device_read_function read;
    vfs_device_write_function write;
} vfs_device_ops;

typedef struct vfs_device
{
    const char* name;
    const vfs_device_ops* ops;

    uint32 sector_size;
    uint64 num_sectors;

    struct vfs_fs_type* fs;
    struct vfs_node* root_node;

    struct vfs_node* mountpoint;

    void* dev_extra;
    void* fs_extra;

    struct vfs_device* next;
} vfs_device;

typedef struct
{
    vfs_fs_try_read_function try_read;
    vfs_fs_destroy_function destroy;

    vfs_fs_load_function load;
    vfs_fs_unload_function unload;

    vfs_fs_create_file_function create_file;
    vfs_fs_create_dir_function create_dir;
    vfs_fs_create_symlink_function create_symlink;

    vfs_fs_read_function read;
    vfs_fs_write_function write;
    vfs_fs_read_symlink_function read_symlink;

    vfs_fs_iter_function iter;
    vfs_fs_find_function find;

    vfs_fs_move_function move;
    vfs_fs_link_function link;
    vfs_fs_unlink_function unlink;
} vfs_fs_ops;

typedef struct vfs_fs_type
{
    const char* name;
    const vfs_fs_ops* ops;
    bool is_generic;

    struct vfs_fs_type* next;
} vfs_fs_type;

typedef struct vfs_node
{
    refcounter refcount;

    vfs_device* dev;
    const vfs_fs_ops* ops;

    mutex lock;

    uint64 inode_no;

    uint32 flags;
    uint64 size;

    void* extra;

    // Note: Doxygen chokes on this unnamed union, so we just tell doxygen to ignore the beginning
    //       and end so it sees the contained members as members of the struct, not the unnamed
    //       union.
    /// \cond
    union
    {
    /// \endcond
        struct vfs_device* mounted;
    /// \cond
    };
    /// \endcond
} vfs_node;

typedef struct {
    mutex lock;

    size_t size;
    vfs_node** nodes;
} vfs_node_cache;

typedef struct vfs_dirent
{
    char name[VFS_NAME_LENGTH + 1];
    uint64 inode_no;
} vfs_dirent;

extern vfs_node vfs_root;

extern mutex vfs_list_lock;
extern vfs_fs_type* vfs_type_first;
extern vfs_device* vfs_device_first;

extern uint32 vfs_null_op(void);

/// \cond
extern void vfs_init(const boot_param* param) __hidden;
/// \endcond

extern void vfs_fs_type_register(vfs_fs_type* type);
extern void vfs_fs_type_unregister(vfs_fs_type* type);

extern vfs_device* vfs_device_create(const char* name, const vfs_device_ops* ops, uint32 sector_size, uint64 num_sectors, void* dev_extra) __warn_unused_result;
extern void vfs_device_destroy(vfs_device* dev);

extern uint32 vfs_node_cache_init(vfs_node_cache* cache, size_t size) __warn_unused_result;
extern bool vfs_node_cache_find(vfs_node_cache* cache, uint64 inode_no, vfs_node** node_out) __warn_unused_result;

extern uint32 vfs_normalize_path(const char* cur_path, const char* path_in, char* path_out, size_t* len) __warn_unused_result;
extern uint32 vfs_resolve_path(vfs_node* root, const char* cur_path, const char* path, bool follow_symlinks, vfs_node** node_out, char* canonical_path_out);

extern uint32 vfs_mount(vfs_node* mountpoint, vfs_device* dev) __warn_unused_result;
extern uint32 vfs_unmount(vfs_node* mountpoint, uint32 force);

#endif
