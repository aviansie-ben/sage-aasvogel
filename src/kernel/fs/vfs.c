#define ALLOW_UNSAFE_NODE_REFCOUNT

#include <fs/vfs.h>
#include <fs/initrd.h>
#include <fs/saif.h>

#include <core/klog.h>
#include <core/crash.h>

#include <string.h>

vfs_node vfs_root = {
    .refcount = { .refcount = 1 },
    
    .dev = NULL,
    .ops = NULL,
    
    .inode_no = 0,
    .flags = VFS_TYPE_DIRECTORY,
    
    .size = 0,
    
    .extra = NULL,
    
    .mounted = NULL
};

uint32 vfs_null_op(void)
{
    return E_NOT_SUPPORTED;
}

mutex vfs_list_lock;
vfs_fs_type* vfs_type_first;
vfs_device* vfs_device_first;

static mempool_small vfs_node_mempool;

void vfs_init(const boot_param* param)
{
    // Initialize the memory pools
    kmem_pool_small_init(&vfs_node_mempool, "vfs_node", sizeof(vfs_node), __alignof__(vfs_node), 0);
    
    vfs_device* initrd_dev;
    
    saif_init();
    if (initrd_create(param, &initrd_dev) != 0 || vfs_mount(&vfs_root, initrd_dev) != 0)
        crash("Failed to mount initrd");
}

void vfs_fs_type_register(vfs_fs_type* type)
{
    type->next = vfs_type_first;
    vfs_type_first = type;
}

void vfs_fs_type_unregister(vfs_fs_type* type)
{
    if (vfs_type_first == type)
    {
        vfs_type_first = type->next;
    }
    else
    {
        vfs_fs_type* prev;
        
        for (prev = vfs_type_first; prev != NULL && prev->next != type; prev = prev->next) ;
        
        if (prev != NULL)
            prev->next = type->next;
    }
}

vfs_device* vfs_device_create(const char* name, const vfs_device_ops* ops, uint32 sector_size, uint64 num_sectors, void* dev_extra)
{
    vfs_device* dev = kmem_pool_generic_alloc(sizeof(vfs_device), 0);
    
    if (dev != NULL)
    {
        dev->name = name;
        dev->ops = ops;
        
        dev->sector_size = sector_size;
        dev->num_sectors = num_sectors;
        
        dev->fs = NULL;
        dev->root_node = NULL;
        
        dev->mountpoint = NULL;
        
        dev->dev_extra = dev_extra;
        dev->fs_extra = NULL;
        
        dev->next = vfs_device_first;
        vfs_device_first = dev;
        
        return dev;
    }
    else
    {
        return NULL;
    }
}

void vfs_device_destroy(vfs_device* dev)
{
    vfs_device* pdev;
    
    // If the device currently has a filesystem, we must unload it before we can destroy the
    // device itself.
    if (dev->fs != NULL)
    {
        if (dev->mountpoint != NULL)
            vfs_unmount(dev->mountpoint, 2);
        else
            dev->fs->ops->destroy(dev, true);
    }
    
    // Now we can unlink the device from the list of known devices
    if (vfs_device_first == dev)
    {
        vfs_device_first = dev->next;
    }
    else
    {
        for (pdev = vfs_device_first; pdev != NULL && pdev->next != dev; pdev = pdev->next) ;
        
        if (pdev != NULL)
            pdev->next = dev->next;
        else
            crash("Attempt to destroy a device that was never registered!");
    }
    
    // And now we can finally free the memory allocated for the device
    kmem_pool_generic_free(dev);
}

uint32 vfs_node_cache_init(vfs_node_cache* cache, size_t size)
{
    mutex_init(&cache->lock);
    
    cache->size = size;
    cache->nodes = kmem_pool_generic_alloc(sizeof(*cache->nodes) * size, 0);
    
    if (cache->nodes == NULL)
    {
        return E_NO_MEMORY;
    }
    
    for (size_t i = 0; i < size; i++)
        cache->nodes[i] = NULL;
    
    return E_SUCCESS;
}

bool vfs_node_cache_find(vfs_node_cache* cache, uint64 inode_no, vfs_node** node_out)
{
    vfs_node** evict = NULL;
    
    for (size_t i = 0; i < cache->size; i++)
    {
        if (cache->nodes[i] != NULL && cache->nodes[i]->inode_no == inode_no)
        {
            refcount_inc_unsafe(&cache->nodes[i]->refcount);
            *node_out = cache->nodes[i];
            return true;
        }
        
        if ((evict == NULL || *evict != NULL) && cache->nodes[i] == NULL)
        {
            evict = &cache->nodes[i];
        }
        else if (evict == NULL && cache->nodes[i] != NULL && cache->nodes[i]->refcount.refcount == 0)
        {
            evict = &cache->nodes[i];
        }
    }
    
    if (evict == NULL)
    {
        // TODO Try to grow the size of the cache
        *node_out = NULL;
        return false;
    }
    
    if (*evict != NULL)
    {
        if ((*evict)->ops != NULL)
            (*evict)->ops->unload(*evict);
        
        (*evict)->refcount.refcount = 1;
    }
    else
    {
        *evict = kmem_pool_small_alloc(&vfs_node_mempool, 0);
        
        if (*evict == NULL)
        {
            *node_out = NULL;
            return false;
        }
        
        refcount_init(&(*evict)->refcount, NULL);
        mutex_init(&(*evict)->lock);
    }
    
    (*evict)->inode_no = inode_no;
    
    *node_out = *evict;
    return false;
}

uint32 vfs_normalize_path(const char* cur_path, const char* path_in, char* path_out, size_t* len)
{
    char path_part[VFS_NAME_LENGTH]; // Note: No null terminator
    size_t i = 0;
    size_t j = 0;
    size_t k;
    
    if (*path_in == VFS_SEPARATOR_CHAR)
    {
        *path_out++ = VFS_SEPARATOR_CHAR;
        path_in++;
        i++;
    }
    else
    {
        while (*cur_path != '\0' && i != VFS_PATH_LENGTH)
        {
            *path_out++ = *cur_path++;
            i++;
        }
    }
    
    while (i < VFS_PATH_LENGTH)
    {
        if (*path_in == VFS_SEPARATOR_CHAR || *path_in == '\0')
        {
            if (j == 0 && *path_in == '\0') break;
            
            if (j == 1 && path_part[0] == '.')
            {
                // Ignore
            }
            else if (j == 2 && path_part[0] == '.' && path_part[1] == '.')
            {
                // We need to move back until we find the previous directory
                if (i != 1)
                {
                    path_out--;
                    i--;
                    
                    while (i != 1 && *(path_out - 1) != VFS_SEPARATOR_CHAR)
                    {
                        path_out--;
                        i--;
                    }
                }
            }
            else
            {
                // Copy the path part to the output
                for (k = 0; k != j && i != VFS_PATH_LENGTH; k++)
                {
                    *path_out++ = path_part[k];
                    i++;
                }
                
                if (*path_in == VFS_SEPARATOR_CHAR)
                {
                    if (i != VFS_PATH_LENGTH)
                    {
                        // Add a separator to the path
                        *path_out++ = VFS_SEPARATOR_CHAR;
                        i++;
                    }
                    else
                    {
                        // The path has exceeded the maximum allowable path length!
                        // Replace the last character with a path separator.
                        *(path_out - 1) = VFS_SEPARATOR_CHAR;
                    }
                }
            }
            
            if (*path_in == '\0') break;
            
            path_in++;
            j = 0;
        }
        else
        {
            if (j == VFS_NAME_LENGTH)
                return E_NAME_TOO_LONG;
            
            path_part[j++] = *path_in++;
        }
    }
    
    if (i == VFS_PATH_LENGTH)
        return E_NAME_TOO_LONG;
    
    if (len != NULL)
        *len = i;
    
    *path_out = '\0';
    return E_SUCCESS;
}

uint32 vfs_resolve_path(vfs_node* root, const char* cur_path, const char* path_in, bool follow_symlinks, vfs_node** node_out, char* canonical_path_out)
{
    uint32 symlinks = 0;
    
    char path[VFS_PATH_LENGTH + 1];
    char path_part[VFS_NAME_LENGTH + 1];
    
    vfs_node* node = root;
    vfs_node* next_node;
    
    size_t i = 1;
    size_t j = 0;
    size_t k;
    
    uint32 err;
    
    void resolve_node(void)
    {
        while (true)
        {
            mutex_acquire(&node->lock);
            if ((node->flags & VFS_FLAG_MOUNTPOINT) != 0)
            {
                next_node = node->mounted->root_node;
                
                refcount_inc(&next_node->refcount);
                
                mutex_release(&node->lock);
                refcount_dec(&node->refcount);
                
                node = next_node;
            }
            else if (VFS_TYPE(node->flags) == VFS_TYPE_SYMLINK)
            {
                char symlink_path[VFS_PATH_LENGTH + 1];
                char old_path[VFS_PATH_LENGTH + 1];
                
                // Make sure we don't follow too many symlinks
                if (symlinks++ == VFS_SYMLINK_DEPTH)
                {
                    mutex_release(&node->lock);
                    refcount_dec(&node->refcount);
                    
                    err = E_LOOP;
                    return;
                }
                
                // Read the symlink and discard the node reference
                if (node->ops != NULL)
                    err = node->ops->read_symlink(node, symlink_path);
                else
                    err = E_IO_ERROR; // The node was orphaned during our search
                
                mutex_release(&node->lock);
                refcount_dec(&node->refcount);
                
                if (err != E_SUCCESS)
                    return;
                
                // Read the remainder of the path beyond the symlink
                i++;
                for (k = 0; path[i] != '\0'; i++, k++)
                    old_path[k] = path[i];
                old_path[k] = '\0';
                
                // Add a null terminator in the proper location so that we get
                // the path of the symlink
                i -= k;
                if (i != 1)
                {
                    while (path[--i - 1] != VFS_SEPARATOR_CHAR) ;
                }
                
                path[i] = '\0';
                
                // Normalize the symlink path
                err = vfs_normalize_path(path, symlink_path, path, &i);
                
                if (err != E_SUCCESS)
                    return;
                
                // If the symlink didn't end with a separator, make sure to add
                // one.
                if (path[i - 1] != VFS_SEPARATOR_CHAR && i != VFS_PATH_LENGTH)
                    path[i++] = VFS_SEPARATOR_CHAR;
                
                // Concatenate the remainder of the old path with the new path
                // found from the symlink.
                for (k = 0; old_path[k] != '\0' && i != VFS_PATH_LENGTH; k++, i++)
                    path[i] = old_path[k];
                path[i++] = '\0';
                
                // Make sure we didn't run out of space while concatenating.
                if (i == VFS_PATH_LENGTH)
                {
                    err = E_NAME_TOO_LONG;
                    return;
                }
                
                // Return back to the root node and start resolving nodes again
                // with the new path.
                i = 0;
                node = root;
                refcount_inc(&node->refcount);
            }
            else
            {
                mutex_release(&node->lock);
                
                err = E_SUCCESS;
                return;
            }
        }
    }
    
    err = vfs_normalize_path(cur_path, path_in, path, NULL);
    
    if (err != E_SUCCESS)
        return err;

    refcount_inc(&node->refcount);
    
    // We need to try to resolve any links on the root node.
    resolve_node();
    if (err != E_SUCCESS)
        return err;
    
    while (path[i] != '\0')
    {
        if (path[i] == VFS_SEPARATOR_CHAR)
        {
            // Find the next node along the path
            path_part[j] = '\0';
            
            mutex_acquire(&node->lock);
            if (node->ops != NULL)
                err = node->ops->find(node, path_part, &next_node);
            else
                err = E_IO_ERROR; // The node was orphaned during our search
            mutex_release(&node->lock);
            
            refcount_dec(&node->refcount);
            node = next_node;
            
            if (err != E_SUCCESS)
                return err;
            
            // Fully resolve the new node, following any links
            resolve_node();
            if (err != E_SUCCESS)
                return err;
            
            // Make sure that we're now pointing at a directory
            if (VFS_TYPE(node->flags) != VFS_TYPE_DIRECTORY)
            {
                refcount_dec(&node->refcount);
                return E_NOT_DIR;
            }
            
            j = 0;
        }
        else
        {
            if (j == VFS_NAME_LENGTH)
            {
                refcount_dec(&node->refcount);
                return E_NAME_TOO_LONG;
            }
            
            path_part[j++] = path[i];
        }
        
        i++;
    }
    
    // Follow the final part of the path. No links will be resolved here.
    if (j != 0)
    {
        path_part[j] = '\0';
        
        mutex_acquire(&node->lock);
        if (node->ops != NULL)
            err = node->ops->find(node, path_part, &next_node);
        else
            err = E_IO_ERROR; // The node was orphaned during our search
        mutex_release(&node->lock);
        
        refcount_dec(&node->refcount);
        node = next_node;
        
        if (err != E_SUCCESS)
            return err;
    }
    
    if (node_out != NULL)
        *node_out = node;
    else
        refcount_dec(&node->refcount);
    
    if (canonical_path_out != NULL)
    {
        strcpy(canonical_path_out, path);
    }
    
    
    return E_SUCCESS;
}

uint32 vfs_mount(vfs_node* mountpoint, vfs_device* dev)
{
    uint32 errno;
    if (VFS_TYPE(mountpoint->flags) != VFS_TYPE_DIRECTORY)
        return E_NOT_DIR;
    
    if (dev->fs == NULL)
    {
        vfs_fs_type* t = vfs_type_first;
        
        while (t != NULL)
        {
            errno = t->ops->try_read(t, dev);
            
            if (errno == E_SUCCESS)
            {
                break;
            }
            else if (errno == E_IO_ERROR)
            {
                return E_IO_ERROR;
            }
            else if (errno == E_NO_MEMORY)
            {
                return E_NO_MEMORY;
            }
            
            t = t->next;
        }
        
        if (t == NULL)
        {
            return E_INVALID;
        }
    }
    else if (dev->mountpoint != NULL)
    {
        return E_BUSY;
    }
    
    mutex_acquire(&mountpoint->lock);
    
    if ((mountpoint->flags & VFS_FLAG_MOUNTPOINT) != 0)
    {
        mutex_release(&mountpoint->lock);
        return E_ALREADY_EXISTS;
    }
    
    dev->mountpoint = mountpoint;
    mountpoint->mounted = dev;
    mountpoint->flags |= VFS_FLAG_MOUNTPOINT;
    
    refcount_inc(&mountpoint->refcount);
    mutex_release(&mountpoint->lock);
    
    return E_SUCCESS;
}

uint32 vfs_unmount(vfs_node* mountpoint, uint32 force)
{
    uint32 errno;
    vfs_device* dev;
    vfs_device* mdev;
    
    // Check that the device is actually a mountpoint
    mutex_acquire(&mountpoint->lock);
    if ((mountpoint->flags & VFS_FLAG_MOUNTPOINT) == 0)
    {
        return E_INVALID;
    }
    dev = mountpoint->mounted;
    mutex_release(&mountpoint->lock);
    
    // If any nodes under this device were themselves mountpoints, we need to unmount those first in
    // order to prevent any orphaned nodes from being impossible to remove.
    for (mdev = vfs_device_first; mdev != NULL; mdev = mdev->next)
    {
        mountpoint = mdev->mountpoint;
        
        if (mountpoint != NULL && mountpoint->dev == dev)
        {
            errno = vfs_unmount(mountpoint, (errno == 0) ? 0 : 1);
            
            if (force == 0 && errno != E_SUCCESS)
                return errno;
        }
    }
    
    // Try to actually destroy the filesystem information and unmount the filesystem
    errno = dev->fs->ops->destroy(dev, force >= 2);
    
    if (errno != E_SUCCESS)
    {
        if (force == 0)
            return errno;
        
        // Unmount the filesystem from its mountpoint anyway
        mutex_acquire(&mountpoint->lock);
        dev->mountpoint = NULL;
        mountpoint->flags &= (uint32)~VFS_FLAG_MOUNTPOINT;
        mountpoint->mounted = NULL;
        mutex_release(&mountpoint->lock);
        refcount_dec(&mountpoint->refcount);
        
        if (force == 1)
            return errno;
    }
    
    return E_SUCCESS;
}
