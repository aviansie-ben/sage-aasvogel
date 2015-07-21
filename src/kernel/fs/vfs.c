#include <fs/vfs.h>

#include <core/crash.h>

#include <string.h>

vfs_node vfs_root = {
    .dev = NULL,
    .ops = NULL,
    
    .inode_no = 0,
    .refcount = 1,
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
fs_type* vfs_type_first;
fs_device* vfs_device_first;

mempool_small vfs_node_pool;
mempool_small vfs_device_pool;

void vfs_init(const boot_param* param)
{
    // Initialize the memory pools
    kmem_pool_small_init(&vfs_node_pool, "vfs_node", sizeof(vfs_node), __alignof__(vfs_node));
    kmem_pool_small_init(&vfs_device_pool, "fs_device", sizeof(fs_device), __alignof__(fs_device));
    
    // TODO: Mount initrd
}

void vfs_register_fs(fs_type* type)
{
    type->next = vfs_type_first;
    vfs_type_first = type;
}

void vfs_unregister_fs(fs_type* type)
{
    if (vfs_type_first == type)
    {
        vfs_type_first = type->next;
    }
    else
    {
        fs_type* prev;
        
        for (prev = vfs_type_first; prev != NULL && prev->next != type; prev = prev->next) ;
        
        if (prev != NULL)
            prev->next = type->next;
    }
}

fs_device* vfs_create_device(const char* name, const fs_device_ops* ops, uint32 sector_size, uint64 num_sectors, void* dev_extra)
{
    fs_device* dev = kmem_pool_small_alloc(&vfs_device_pool, 0);
    
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

void vfs_destroy_device(fs_device* dev)
{
    crash("Not implemented!");
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
    vfs_node* next_node = NULL;
    
    size_t i = 1;
    size_t j = 0;
    size_t k;
    
    uint32 err;
    
    err = vfs_normalize_path(cur_path, path_in, path, NULL);
    
    if (err != E_SUCCESS)
        return err;
    
    vfs_node_ref(root);
    
    while (path[i] != '\0')
    {
        if (path[i] == VFS_SEPARATOR_CHAR)
        {
            // Find the next node along the path
            path_part[j] = '\0';
            err = node->ops->find(node, path_part, &next_node);
            vfs_node_unref(node);
            node = next_node;
            
            if (err != E_SUCCESS)
                return err;
            
            // If the next node is a mountpoint, follow it
            mutex_acquire(&node->lock);
            if ((node->flags & VFS_FLAG_MOUNTPOINT) != 0)
            {
                next_node = node->mounted->root_node;
                
                vfs_node_ref(next_node);
                vfs_node_unref(node);
                
                mutex_release(&node->lock);
                node = next_node;
            }
            else
            {
                mutex_release(&node->lock);
            }
            
            // If the next node is a symlink, it should be resolved
            if (follow_symlinks && VFS_TYPE(node->flags) == VFS_TYPE_SYMLINK)
            {
                char symlink_path[VFS_PATH_LENGTH + 1];
                char old_path[VFS_PATH_LENGTH + 1];
                
                // Make sure we don't follow too many symlinks
                if (symlinks++ == VFS_SYMLINK_DEPTH)
                {
                    vfs_node_unref(node);
                    return E_LOOP;
                }
                
                // Read the symlink and discard the node reference
                err = node->ops->read_symlink(node, symlink_path);
                vfs_node_unref(node);
                
                if (err != E_SUCCESS)
                    return err;
                
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
                    return err;
                
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
                    return E_NAME_TOO_LONG;
                
                // Return back to the root node and start resolving nodes again
                // with the new path.
                i = 0;
                node = root;
                vfs_node_ref(root);
            }
            
            // Make sure that we're now pointing at a directory
            if (VFS_TYPE(node->flags) != VFS_TYPE_DIRECTORY)
            {
                vfs_node_unref(node);
                return E_NOT_DIR;
            }
            
            j = 0;
        }
        else
        {
            if (j == VFS_NAME_LENGTH)
            {
                vfs_node_unref(node);
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
        
        err = node->ops->find(node, path_part, &next_node);
        vfs_node_unref(node);
        node = next_node;
        
        if (err != E_SUCCESS)
            return err;
    }
    
    if (node_out != NULL)
        *node_out = node;
    else
        vfs_node_unref(node);
    
    if (canonical_path_out != NULL)
    {
        strcpy(canonical_path_out, path);
    }
    
    
    return E_SUCCESS;
}

void vfs_node_ref(vfs_node* node)
{
    if (__atomic_fetch_add(&node->refcount, 1, __ATOMIC_SEQ_CST) == 0)
        crash("vfs_node refcount race detected!");
}

void vfs_node_unref(vfs_node* node)
{
    uint32 refcount = __atomic_sub_fetch(&node->refcount, 1, __ATOMIC_SEQ_CST);
    
    if (refcount == 0)
        node->ops->unload(node);
    else if (refcount == (uint32)-1)
        crash("vfs_node refcount race detected!");
}

uint32 vfs_mount(vfs_node* mountpoint, fs_device* dev)
{
    uint32 errno;
    if (VFS_TYPE(mountpoint->flags) != VFS_TYPE_DIRECTORY)
        return E_NOT_DIR;
    
    if (dev->fs == NULL)
    {
        fs_type* t = vfs_type_first;
        
        while (t != NULL)
        {
            errno = t->ops->try_read(t, dev);
            
            if (errno == E_SUCCESS)
            {
                break;
            }
            else if (errno == E_IO_ERROR)
            {
                mutex_release(&vfs_list_lock);
                return E_IO_ERROR;
            }
            else if (errno == E_NO_MEMORY)
            {
                mutex_release(&vfs_list_lock);
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
    
    vfs_node_ref(mountpoint);
    mutex_release(&mountpoint->lock);
    
    return E_SUCCESS;
}

void vfs_unmount(vfs_node* mountpoint)
{
    if ((mountpoint->flags & VFS_FLAG_MOUNTPOINT) == 0)
    {
        return;
    }
    
    mountpoint->mounted->mountpoint = NULL;
    mountpoint->mounted = NULL;
    mountpoint->flags &= (uint32)~VFS_FLAG_MOUNTPOINT;
}
