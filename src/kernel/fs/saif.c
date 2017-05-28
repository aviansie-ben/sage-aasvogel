#include <lock/rwlock.h>
#include <core/klog.h>

#include <fs/saif.h>
#include <fs/vfs.h>

#include <assert.h>
#include <string.h>

#define SAIF_SECTOR_SIZE 64
#define SAIF_CACHE_SIZE 8
#define SAIF_NAME_MAX 50
#define SAIF_REVISION 0

#define SAIF_MAGIC_NUM 4

#define SAIF_TYPE_FILE      0x00
#define SAIF_TYPE_DIRECTORY 0x01

typedef struct
{
    char magic[SAIF_MAGIC_NUM];
    uint32 revision;
    uint32 header_checksum;
    
    uint32 root_dir_sector;
    uint32 root_dir_length;
    uint32 root_dir_checksum;
} saif_superblock;

typedef struct
{
    char name[SAIF_NAME_MAX + 1];
    uint8 type;
    
    uint32 sector;
    uint32 length;
    uint32 checksum;
} saif_dirent;

typedef struct
{
    saif_superblock superblock;
    vfs_node_cache cache;
} saif_fs_info;

static const char _saif_magic[SAIF_MAGIC_NUM] = { 'S', 'A', 'I', 'F' };

static const vfs_fs_ops _saif_ops;
static vfs_fs_type _saif_type = {
    .name = "SAIF",
    .ops = &_saif_ops,
    
    .is_generic = true,
    
    .next = NULL
};

static uint32 _saif_map_type(uint32 type)
{
    if (type == SAIF_TYPE_DIRECTORY)
    {
        return VFS_TYPE_DIRECTORY;
    }
    else
    {
        return VFS_TYPE_FILE;
    }
}

static uint32 _saif_get_node(vfs_device* dev, uint32 sector, uint32 length, uint32 type, vfs_node** vnode);
static void _saif_unload_node(vfs_node* node);
static uint32 _saif_read_node(vfs_node* node, uint64 offset, size_t length, uint8* buffer);
static uint32 _saif_iter_node(vfs_node* parent, uint32 offset, vfs_dirent* dirent);
static uint32 _saif_find_node(vfs_node* parent, const char* name, vfs_node** child);

static uint32 _saif_try_read(struct vfs_fs_type* type, struct vfs_device* dev)
{
    if (dev->sector_size != SAIF_SECTOR_SIZE)
        return E_INVALID;
    
    uint32 errno;
    uint8 buf[SAIF_SECTOR_SIZE];
    saif_superblock* sb = (saif_superblock*)buf;
    
    if ((errno = dev->ops->read(dev, 0, 1, buf)) != E_SUCCESS)
        return errno;
    
    for (uint32 i = 0; i < SAIF_MAGIC_NUM; i++)
    {
        if (sb->magic[i] != _saif_magic[i])
            return E_INVALID;
    }
    
    if (sb->root_dir_sector + ((sb->root_dir_length + SAIF_SECTOR_SIZE - 1) / SAIF_SECTOR_SIZE) > dev->num_sectors || sb->root_dir_sector > dev->num_sectors)
    {
        klog(KLOG_LEVEL_WARN, "saif: filesystem on %s has out-of-bounds root directory\n");
        return E_INVALID;
    }
    
    if (sb->revision != SAIF_REVISION)
    {
        klog(KLOG_LEVEL_WARN, "saif: filesystem on %s uses an unrecognized SAIF revision\n");
        return E_INVALID;
    }
    
    saif_fs_info* info;
    if ((info = kmem_pool_generic_alloc(sizeof(saif_fs_info), 0)) == NULL)
        return E_NO_MEMORY;
    
    info->superblock = *sb;
    errno = vfs_node_cache_init(&info->cache, SAIF_CACHE_SIZE);
    if (errno != E_SUCCESS)
    {
        kmem_pool_generic_free(info);
        return errno;
    }
    
    dev->fs_extra = info;
    
    errno = _saif_get_node(dev, sb->root_dir_sector, sb->root_dir_length, VFS_TYPE_DIRECTORY, &dev->root_node);
    if (errno != E_SUCCESS)
    {
        kmem_pool_generic_free(dev->fs_extra);
        return errno;
    }
    
    return E_SUCCESS;
}

static uint32 _saif_destroy(struct vfs_device* dev, bool force)
{
    assert(false); // TODO Implement this
}

static const vfs_fs_ops _saif_ops = {
    .try_read = _saif_try_read,
    .destroy = _saif_destroy,
    
    .load = (vfs_fs_load_function) vfs_null_op,
    .unload = _saif_unload_node,
    
    .create_file = (vfs_fs_create_file_function) vfs_null_op,
    .create_dir = (vfs_fs_create_dir_function) vfs_null_op,
    .create_symlink = (vfs_fs_create_symlink_function) vfs_null_op,
    
    .read = _saif_read_node,
    .write = (vfs_fs_write_function) vfs_null_op,
    .read_symlink = (vfs_fs_read_symlink_function) vfs_null_op,
    
    .iter = _saif_iter_node,
    .find = _saif_find_node,
    
    .move = (vfs_fs_move_function) vfs_null_op,
    .link = (vfs_fs_link_function) vfs_null_op,
    .unlink = (vfs_fs_unlink_function) vfs_null_op
};

static uint32 _saif_get_node(vfs_device* dev, uint32 sector, uint32 length, uint32 type, vfs_node** node_out)
{
    saif_fs_info* info = (saif_fs_info*) dev->fs_extra;
    vfs_node* node;
    
    mutex_acquire(&info->cache.lock);
    if (!vfs_node_cache_find(&info->cache, sector, &node))
    {
        if (node == NULL)
        {
            mutex_release(&info->cache.lock);
            return E_NO_MEMORY;
        }
        
        mutex_acquire(&node->lock);
        mutex_release(&info->cache.lock);
        
        node->dev = dev;
        node->ops = &_saif_ops;
        node->flags = type;
        node->size = length;
        
        mutex_release(&node->lock);
    }
    else
    {
        mutex_release(&info->cache.lock);
    }
    
    *node_out = node;
    return E_SUCCESS;
}

static void _saif_unload_node(vfs_node* node)
{
    // Do nothing
}

static uint32 _saif_read_node(vfs_node* node, uint64 offset, size_t length, uint8* buffer_out)
{
    if (offset >= node->size || length > node->size - offset)
        return E_INVALID;
    
    uint64 first_sector = offset / SAIF_SECTOR_SIZE + node->inode_no;
    uint64 num_sectors = (length + offset % SAIF_SECTOR_SIZE + SAIF_SECTOR_SIZE - 1) / SAIF_SECTOR_SIZE;
    
    uint32 errno;
    uint8 buffer[num_sectors * SAIF_SECTOR_SIZE];
    
    errno = node->dev->ops->read(
        node->dev,
        first_sector,
        (uint32)num_sectors,
        buffer
    );
    
    if (errno == E_SUCCESS)
        memcpy(buffer_out, &buffer[offset % SAIF_SECTOR_SIZE], length);
    
    return errno;
}

static uint32 _saif_read_dirent(vfs_node* parent, uint32 offset, saif_dirent* dirent)
{
    if (VFS_TYPE(parent->flags) != VFS_TYPE_DIRECTORY)
        return E_NOT_DIR;
    else if (offset * sizeof(saif_dirent) >= parent->size)
        return E_INVALID;
    
    return _saif_read_node(parent, (uint64)offset * sizeof(saif_dirent), sizeof(saif_dirent), (uint8*) dirent);
}

static uint32 _saif_iter_node(vfs_node* parent, uint32 offset, vfs_dirent* dirent)
{
    saif_dirent dirent_internal;
    uint32 errno;
    
    errno = _saif_read_dirent(parent, offset, &dirent_internal);
    if (errno != E_SUCCESS)
        return errno;
    
    strcpy(dirent->name, dirent_internal.name);
    dirent->inode_no = dirent_internal.sector;
    
    return E_SUCCESS;
}

static uint32 _saif_find_node(vfs_node* parent, const char* name, vfs_node** child)
{
    uint32 errno;
    saif_dirent dirent;
    
    if (VFS_TYPE(parent->flags) != VFS_TYPE_DIRECTORY)
        return E_NOT_DIR;
    
    for (uint32 i = 0; i * sizeof(saif_dirent) < parent->size; i++)
    {
        errno = _saif_read_dirent(parent, i, &dirent);
        if (errno != E_SUCCESS)
            return errno;
        
        if (strcmp(name, dirent.name) == 0)
        {
            return _saif_get_node(parent->dev, dirent.sector, dirent.length, _saif_map_type(dirent.type), child);
        }
    }
    
    return E_NOT_FOUND;
}

void saif_init(void)
{
    mutex_acquire(&vfs_list_lock);
    vfs_fs_type_register(&_saif_type);
    mutex_release(&vfs_list_lock);
}
