/**
 * \file
 * TODO: Add file description
 */

#ifndef FS_VFS_H
#define FS_VFS_H

#include <typedef.h>
#include <lock/mutex.h>
#include <memory/pool.h>
#include <core/bootparam.h>

/**
 * \brief The character used to separate directories in pathnames.
 */
#define VFS_SEPARATOR_CHAR '/'

/**
 * \brief The character used to separate directories in pathnames as a null-terminated string.
 */
#define VFS_SEPARATOR      "/"

/**
 * \brief The maximum length (excluding null terminator) of any file or directory.
 */
#define VFS_NAME_LENGTH 255

/**
 * \brief The maximum length (excluding null terminator) of a path that the system can handle.
 */
#define VFS_PATH_LENGTH 1023

/**
 * \brief The maximum number of symbolic links that will be followed when trying to resolve a path.
 * 
 * This limit is in place to prevent denial of service on other processes as well as to ensure that
 * the kernel does not loop infinitely trying to resolve a symlink with a cycle.
 */
#define VFS_SYMLINK_DEPTH 20

/**
 * \brief Gets the portion of \link vfs_node::flags \endlink which represents the type of node that
 *        is being examined.
 */
#define VFS_TYPE(f)          ((f) & 0xf)

/**
 * \brief Represents a node that is a regular file, as returned by \link VFS_TYPE \endlink.
 */
#define VFS_TYPE_FILE        0x00

/**
 * \brief Represents a node that is a directory, as returned by \link VFS_TYPE \endlink.
 */
#define VFS_TYPE_DIRECTORY   0x01

/**
 * \brief Represents a node that is a symbolic link, as returned by \link VFS_TYPE \endlink.
 */
#define VFS_TYPE_SYMLINK     0x02

/**
 * \brief A flag denoting that this node is a mountpoint on which another filesystem has been
 *        mounted.
 * 
 * This flag should only ever be applied to directories.
 */
#define VFS_FLAG_MOUNTPOINT  0x10

struct fs_type;
struct fs_device;

struct vfs_node;
struct vfs_dirent;

/**
 * \brief Reads a number of contiguous sectors from this filesystem device and places them into a
 *        buffer.
 * 
 * \param[in] dev The device from which sectors should be read.
 * 
 * \param[in] sector The number of the first sector which should be read from the device.
 * 
 * \param[in] num_sectors The number of contiguous sectors which should be read into the buffer.
 * 
 * \param[out] buffer The buffer into which the data should be read. The buffer must be at least
 *                    dev->sector_size * num_sectors bytes in length. If an error is encountered
 *                    while reading data, this buffer is left in an undefined state.
 * 
 * \retval E_SUCCESS The operation was successful.
 * \retval E_IO_ERROR An I/O error was encountered while reading from the device.
 * \retval E_INVALID One or more of the requested sectors was out of range.
 * \retval E_NO_MEMORY There is insufficient memory to allocate a DMA read buffer.
 */
typedef __warn_unused_result uint32 (*fs_device_read)(struct fs_device* dev, uint64 sector, size_t num_sectors, uint8* buffer);

/**
 * \brief Writes a number of contiguous sectors to this filesystem device from the provided buffer.
 * 
 * \param[in] dev The device to which the provided sectors should be written.
 * 
 * \param[in] sector The number of the first sector to be written to the device.
 * 
 * \param[in] num_sectors The number of contiguous sectors which should be written to the device.
 * 
 * \param[in] buffer The buffer which holds the data to be written to the device. The buffer must
 *                   be dev->sector_size * num_sectors bytes in length. This buffer must not be
 *                   changed at any time during the call.
 * 
 * \retval E_SUCCESS The operation was successful.
 * \retval E_IO_ERROR An I/O error was encountered while writing to the device.
 * \retval E_INVALID One or more of the requested sectors was out of range.
 * \retval E_NO_MEMORY There is insufficient memory to allocate a DMA write buffer.
 */
typedef __warn_unused_result uint32 (*fs_device_write)(struct fs_device* dev, uint64 sector, size_t num_sectors, const uint8* buffer);

/**
 * \brief Attempts to read a filesystem of this type off of a generic block device.
 * 
 * If this device can be read using the given filesystem, the device will be read using that
 * filesystem and dev->fs and dev->root_node will be initialized.
 * 
 * This operation may not be supported on all filesystems. Filesystems which do not operate on a
 * generic block device generally won't support this function.
 * 
 * \param[in] type The type of filesystem to attempt to read.
 * 
 * \param[in] device The device to attempt to read using the given filesystem.
 * 
 * \retval E_SUCCESS The device was successfully read using the given filesystem.
 * \retval E_NOT_SUPPORTED This filesystem type does not support reading from generic block devices.
 * \retval E_IO_ERROR An I/O error was encountered while trying to read from the device.
 * \retval E_INVALID The device does not appear to be readable using the given filesystem.
 * \retval E_NO_MEMORY There is insufficient memory to read from the device or initialize the
 *                     filesystem.
 */
typedef __warn_unused_result uint32 (*vfs_try_read_function)(struct fs_type* type, struct fs_device* dev);

/**
 * \brief Unloads any and all information stored by a filesystem on a particular device before the
 *        filesystem is unmounted.
 * 
 * This operation must be supported by all filesystems.
 * 
 * If any nodes in the filesystem are currently locked, and the device is not being forcibly
 * unmounted, then this function **should not** perform any filesystem unloading. If the device
 * is being forcibly unmounted, then these locks should be forcibly released and any read or
 * write operations should be interrupted gracefully and return \link E_IO_ERROR \endlink without
 * crashing the system.
 * 
 * Any vfs nodes within the filesystem which are still being referenced (their refcount is not 0)
 * must not be unloaded. They should be orphaned instead.
 * 
 * If the filesystem information is successfully discarded, it is then the responsibility of this
 * function to unlink this device from its mountpoint in such a way that no other thread may see
 * an inconsistent mount, i.e. a device mounted which has no filesystem.
 * 
 * \param[in] device The device which is being unmounted.
 * 
 * \param[in] force If true, then the unmount is being forced, and this function must ignore any
 *                  errors and continue destroying any information.
 * 
 * \retval E_SUCCESS The filesystem was successfully unloaded and the device is ready to unmount.
 * \retval E_IO_ERROR An I/O error was encountered while unloading the filesystem and the unmount is
 *                    not being forced.
 * \retval E_BUSY One or more files on this filesystem are currently locked and the unmount is not
 *                being forced.
 * \retval E_NO_MEMORY There is insufficient memory to cleanly unmount the filesystem and the
 *                     unmount is not being forced.
 */
typedef uint32 (*vfs_destroy_function)(struct fs_device* dev, bool force);

/**
 * \brief Loads a vfs node that has the given inode number from the filesystem.
 * 
 * Support for this function is optional. Filesystems which do not use inodes may not support this
 * function.
 * 
 * If a node with the given inode number is currently referenced elsewhere, a new node **must not**
 * be created. This function shall automatically adjust the refcount of the given node to reflect
 * that a new reference was created. It is the responsibility of the caller to call
 * \link vfs_node_unref \endlink when this reference is discarded.
 * 
 * \param[in] dev The device from which the vfs node should be loaded.
 * 
 * \param[in] inode_no The inode number of the vfs node which should be loaded.
 * 
 * \param[out] node If the node is successfully loaded, this shall be set to the new node.
 * 
 * \retval E_SUCCESS The node was successfully loaded.
 * \retval E_NOT_SUPPORTED This filesystem does not support loading files by inode number.
 * \retval E_IO_ERROR An I/O error was encountered while trying to read the inode.
 * \retval E_NOT_FOUND There is no inode with the given inode number.
 * \retval E_INVALID The inode number given is reserved by the filesystem for some other purpose.
 * \retval E_NO_MEMORY There is insufficient memory to load a new inode.
 */
typedef __warn_unused_result uint32 (*vfs_load_function)(struct fs_device* dev, uint32 inode_no, struct vfs_node** node);

/**
 * \brief Unloads a vfs node which is no longer referenced from memory.
 * 
 * If the requested node's refcount is increased between the time that this function is called and
 * such a time that a lock can be acquired to ensure that no more references can be made, this
 * function must do nothing.
 * 
 * This function is not under any obligation to unload the node immediately. It may choose to
 * unload the given node from the cache at a later time at its discretion.
 * 
 * \param[in] node The node which now has a reference count of 0 that should be unloaded.
 */
typedef void (*vfs_unload_function)(struct vfs_node* node);

/**
 * \brief Creates a new file on the given filesystem under the given directory.
 * 
 * The newly created node shall have a reference count of 1 upon creation. It is up to the caller
 * to call \link vfs_node_unref \endlink when it is done with the file.
 * 
 * \param[in] parent The directory under which the new file should be created.
 * 
 * \param[in] name The name of the file which should be created.
 * 
 * \param[out] child If the node is created successfully, this will be set to the newly created
 *                   node.
 * 
 * \retval E_SUCCESS The new file was created successfully.
 * \retval E_NOT_SUPPORTED This filesystem is mounted as read-only.
 * \retval E_IO_ERROR An I/O error was encountered while creating the new file.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NAME_TOO_LONG The requested name for the new file exceeds the maximum name length
 *                         supported by the filesystem.
 * \retval E_NO_SPACE There is insufficient space on the device to create the new file.
 * \retval E_NO_MEMORY There is insufficient memory to load a new inode.
 * \retval E_ALREADY_EXISTS A node already exists in the given directory with the given name.
 */
typedef __warn_unused_result uint32 (*vfs_create_file_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);

/**
 * \brief Creates a new directory on the given filesystem under the given directory.
 * 
 * The newly created node shall have a reference count of 1 upon creation. It is up to the caller
 * to call \link vfs_node_unref \endlink when it is done with the directory.
 * 
 * \param[in] parent The directory under which the new directory should be created.
 * 
 * \param[in] name The name of the directory which should be created.
 * 
 * \param[out] child If the node is created successfully, this will be set to the newly created
 *                   node.
 * 
 * \retval E_SUCCESS The new directory was created successfully.
 * \retval E_NOT_SUPPORTED This filesystem is mounted as read-only.
 * \retval E_IO_ERROR An I/O error was encountered while creating the directory.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NAME_TOO_LONG The requested name for the new directory exceeds the maximum name length
 *                         supported by the filesystem.
 * \retval E_NO_SPACE There is insufficient space on the device to create a new directory.
 * \retval E_NO_MEMORY There is insufficient memory to load a new inode.
 * \retval E_ALREADY_EXISTS A node already exists in the given directory with the given name.
 */
typedef __warn_unused_result uint32 (*vfs_create_dir_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);

/**
 * \brief Creates a new symlink on the given filesystem.
 * 
 * The newly created node shall have a reference count of 1 upon creation. It is up to the caller
 * to call \link vfs_node_unref \endlink when it is done with the directory.
 * 
 * \param[in] parent The directory under which the new symlink should be created.
 * 
 * \param[in] name The name of the symlink which should be created.
 * 
 * \param[in] target The target of the newly created symlink.
 * 
 * \param[out] child If the node is created successfully, this will be set to the newly created
 *                   node.
 * 
 * \retval E_SUCCESS The new symlink was created successfully.
 * \retval E_NOT_SUPPORTED This filesystem is mounted as read-only.
 * \retval E_IO_ERROR An I/O error was encountered while creating the symlink.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NAME_TOO_LONG The requested name for the new directory exceeds the maximum name length
 *                         supported by the filesystem.
 * \retval E_NO_SPACE There is insufficient space on the device to create a new symlink.
 * \retval E_NO_MEMORY There is insufficient memory to load a new inode.
 * \retval E_ALREADY_EXISTS A node already exists in the given directory with the given name.
 */
typedef __warn_unused_result uint32 (*vfs_create_symlink_function)(struct vfs_node* parent, const char* name, const char* target, struct vfs_node** child);

/**
 * \brief Reads bytes from the file into the provided buffer.
 * 
 * This will read length bytes from the file, starting at offset bytes into the file.
 * 
 * \param[in] node The node from which the data should be read.
 * 
 * \param[in] offset The number of bytes into the file that the read should start from.
 * 
 * \param[in] length The number of bytes that should be read from the file.
 * 
 * \param[out] buffer The buffer into which the data should be read. The provided buffer must be at
 *                   least length bytes in length. If an error is encountered while reading, the
 *                   buffer is left in an undefined state.
 * 
 * \retval E_SUCCESS The data was read from the file successfully.
 * \retval E_NOT_SUPPORTED This node is not a file.
 * \retval E_IO_ERROR An I/O error was encountered while reading data from the file.
 * \retval E_INVALID One or more of the requested bytes was beyond the end of the file. If a partial
 *                   read is desired, it is the duty of the caller to ensure that reads are within
 *                   the file bounds.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read the
 *                     file.
 */
typedef __warn_unused_result uint32 (*vfs_read_function)(struct vfs_node* node, uint64 offset, size_t length, uint8* buffer);

/**
 * \brief Writes bytes into a file from the provided buffer.
 * 
 * This will write length bytes to the file, starting at offset bytes into the file. If offset is
 * less than or equal to node->size and the write would go beyond the end of the file, then the file
 * will grow in order to accomodate the new data.
 * 
 * \param[in] node The node to which data should be written.
 * 
 * \param[in] offset The number of bytes into the file at which the write should begin.
 * 
 * \param[in] length The number of bytes which should be written into the file.
 * 
 * \param[in] The buffer from which data should be written to the file. This buffer must not change
 *            during the write.
 * 
 * \retval E_SUCCESS The data was written to the file successfully.
 * \retval E_NOT_SUPPORTED This node is not a file or the filesystem is mounted as read-only.
 * \retval E_IO_ERROR An I/O error was encountered while writing data to the file.
 * \retval E_INVALID The requested file offset was greater than the file length.
 * \retval E_NO_SPACE The write would require the file to grow, but there is insufficient space on
 *                    the disk to grow the file to accomodate this.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to write to
 *                     the file.
 */
typedef __warn_unused_result uint32 (*vfs_write_function)(struct vfs_node* node, uint64 offset, size_t length, const uint8* buffer);

/**
 * \brief Reads the target location of a symlink.
 * 
 * This will read the data contained within the symlink in order to determine its target location.
 * 
 * \param[in] node The node from which the target should be read.
 * 
 * \param[out] target If the symlink was read successfully, the null-terminated target of the
 *                    symlink will be copied here. The target buffer must be at least
 *                    \link VFS_PATH_LENGTH \endlink + 1 bytes long.
 * 
 * \retval E_SUCCESS The symlink target was read successfully.
 * \retval E_NOT_SUPPORTED This node is not a symlink.
 * \retval E_IO_ERROR An I/O error was encountered while reading the symlink's target.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read the
 *                     symlink target.
 */
typedef __warn_unused_result uint32 (*vfs_read_symlink_function)(struct vfs_node* node, char* target);

/**
 * \brief Gets a particular directory entry from a directory.
 * 
 * \warning The directory entries returned by this function **do not** include the typical . and ..
 *          directories.
 * 
 * \param[in] parent The node which is being iterated.
 * 
 * \param[in] offset The offset of the directory entry to be retrieved.
 * 
 * \retval E_SUCCESS The requested directory entry was successfully retrieved.
 * \retval E_IO_ERROR An I/O error was encountered while reading the directory entry.
 * \retval E_INVALID The requested offset is beyond the end of the directory.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read the
 *                     directory entries.
 */
typedef __warn_unused_result uint32 (*vfs_iter_function)(struct vfs_node* parent, uint32 offset, struct vfs_dirent* dirent);

/**
 * \brief Finds a child node under a directory by its name.
 * 
 * If the child node is found, its reference count is automatically incremented to show the new
 * reference. It is the responsibility of the caller to call \link vfs_node_unref \endlink before
 * discarding the reference.
 * 
 * \param[in] parent The node under which the child is being found.
 * 
 * \param[in] name The name of the child node which is being found.
 * 
 * \param[out] child If successful, this will be set to the child node which was found.
 * 
 * \retval E_SUCCESS The requested child was successfully found.
 * \retval E_NOT_FOUND The directory does not have a child with the requested name.
 * \retval E_IO_ERROR An I/O error was encountered while reading the directory entry.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read the
 *                     directory entries.
 */
typedef __warn_unused_result uint32 (*vfs_find_function)(struct vfs_node* parent, const char* name, struct vfs_node** child);

/**
 * \brief Moves or renames a directory entry.
 * 
 * \param[in] old_parent The directory under which the node to be moved is currently found.
 * 
 * \param[in] old_name The current name of the node which is to be renamed.
 * 
 * \param[in] new_parent The directory that the node should be moved to.
 * 
 * \param[in] new_name The new name that should be applied to the node.
 * 
 * \retval E_SUCCESS The requested child was successfully renamed.
 * \retval E_NOT_FOUND The reqeusted child was not found.
 * \retval E_NOT_SUPPORTED The filesystem is mounted as read-only.
 * \retval E_IO_ERROR An I/O error was encountered while changing the directory entry.
 * \retval E_NOT_DIR Either the old parent or new parent is not a directory.
 * \retval E_NAME_TOO_LONG The new name exceeds the maximum filename length that the filesystem can
 *                         accomodate.
 * \retval E_LOOP Moving this node would create a loop or make it inaccessible.
 * \retval E_NO_SPACE Renaming the requested node would require the directory to grow, but there is
 *                    insufficient space on the disk to accomodate this.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read from
 *                     or write to the directory.
 * \retval E_ALREADY_EXISTS There is already a directory entry under this directory with the
 *                          requested name.
 */
typedef __warn_unused_result uint32 (*vfs_move_function)(struct vfs_node* old_parent, const char* old_name, struct vfs_node* new_parent, const char* new_name);

/**
 * \brief Links an existing node under the given directory, essentially creating a hard link.
 * 
 * \param[in] parent The directory under which the node should be linked.
 * 
 * \param[in] name The name under which the node should be linked.
 * 
 * \param[in] child The node that should be linked.
 * 
 * \retval E_SUCCESS The child was linked into the directory successfully.
 * \retval E_NOT_SUPPORTED The filesystem is mounted as read-only or the node is already linked and
 *                         the filesystem does not support hard links.
 * \retval E_IO_ERROR An I/O error was encountered while adding the new directory entry.
 * \retval E_NOT_DIR The given node is not a directory.
 * \retval E_NAME_TOO_LONG The name under which the node is being linked exceeds the maximum
 *                         filename length that the filesystem can support.
 * \retval E_NO_SPACE Adding a new directory entry would require the directory to grow, but there is
 *                    insufficient space on the disk to accomodate this.
 * \retval E_NO_MEMORY There is insufficient memory to allocate the necessary buffers to read from
 *                     or write to the directory.
 * \retval E_ALREADY_EXISTS There is already a directory entry under this directory with the
 *                          requested name.
 */
typedef __warn_unused_result uint32 (*vfs_link_function)(struct vfs_node* parent, const char* name, struct vfs_node* child);

/**
 * \brief Unlinks a node under the given directory, effectively deleting it.
 * 
 * After being unlinked from all possible reference points, the filesystem is free to decide to
 * delete the given inode on disk and to orphan the vfs node pointing to it at its discretion.
 * 
 * After unlinking, the reference to the vfs node should be immediately discarded, since it is no
 * longer reliable to use that node.
 * 
 * \param[in] parent The directory under which the node should be unlinked.
 */
typedef __warn_unused_result uint32 (*vfs_unlink_function)(struct vfs_node* parent, const char* name);

/**
 * \brief Represents a number of operations that can be performed on a generic filesystem block
 *        device.
 * 
 * \warning All of the function pointers in this struct are **only** valid for the device for which
 *          they were retrieved. Attempting to call the function pointers with any other device
 *          will result in **undefined behaviour**.
 */
typedef struct
{
    fs_device_read read; /**< \brief Read sectors from the device. \sa fs_device_read */
    fs_device_write write; /**< \brief Write sectors to the device. \sa fs_device_write */
} fs_device_ops;

/**
 * \brief Represents a device which can be mounted into the virtual file system.
 * 
 * This may be ageneric block device which can be mounted with a number of different filesystems or
 * it may be a more specialized device for something like NFS.
 */
typedef struct fs_device
{
    /**
     * The name that this device should be referred to by.
     */
    const char* name;
    
    /**
     * The generic block device operations that this device is capable of performing. This may be
     * NULL if this is a specialized filesystem device.
     */
    const fs_device_ops* ops;
    
    /**
     * The size of a single sector on this device. This value is only valid if ops is non-NULL.
     */
    uint32 sector_size;
    
    /**
     * The number of sectors that this device contains. This value is only valid if ops is
     * non-NULL.
     */
    uint64 num_sectors;
    
    /**
     * The type of filesystem which is currently mounted on this device. This may be NULL if this
     * is a generic device which has not yet been probed for a filesystem or which does not have a
     * valid filesystem.
     */
    struct fs_type* fs;
    
    /**
     * The root node of the filesystem on this device. This value is NULL if and only if fs is also
     * NULL.
     */
    struct vfs_node* root_node;
    
    /**
     * The node that this filesystem is currently mounted on. This value is NULL if the filesystem
     * has not yet been mounted.
     */
    struct vfs_node* mountpoint;
    
    /**
     * Extra device-specific information. This value should **never** be relied upon outside of
     * driver code which owns this device. All other code should treat this as a reserved value.
     */
    void* dev_extra;
    
    /**
     * Extra filesystem-specific information. This value should **never** be relied upon outside of
     * the code of the filesystem which was used to mount this device. All other code should treat
     * this as a reserved value.
     */
    void* fs_extra;
    
    /**
     * The next device in the system.
     */
    struct fs_device* next;
} fs_device;

/**
 * \brief Represents a number of operations that can be performed on a VFS node and/or specific
 *        filesystem type.
 * 
 * \warning The function pointers in this structure are **only** valid for the VFS node and
 *          filesystem type for which they were retrieved. Calling these with any other VFS node or
 *          filesystem type will result in **undefined behaviour**.
 */
typedef struct
{
    /**
     * \brief Tries to read this filesystem from a generic filesystem block device.
     * \sa vfs_try_read_function
     */
    vfs_try_read_function try_read;
    
    /**
     * \brief Discards any filesystem-specific information for a device.
     * \sa vfs_destroy_function
     */
    vfs_destroy_function destroy;
    
    /**
     * \brief Loads a vfs node from the filesystem by its inode number.
     * \sa vfs_load_function
     */
    vfs_load_function load;
    
    /**
     * \brief Unloads an unreferenced vfs node on this filesystem.
     * \sa vfs_unload_function
     */
    vfs_unload_function unload;
    
    /**
     * \brief Creates a new file on this filesystem.
     * \sa vfs_create_file_function
     */
    vfs_create_file_function create_file;
    
    /**
     * \brief Creates a new directory on this filesystem.
     * \sa vfs_create_dir_function
     */
    vfs_create_dir_function create_dir;
    
    /**
     * \brief Creates a new symlink on this filesystem.
     * \sa vfs_create_symlink_function
     */
    vfs_create_symlink_function create_symlink;
    
    /**
     * \brief Reads data from a file.
     * \sa vfs_read_function
     */
    vfs_read_function read;
    
    /**
     * \brief Writes data to a file.
     * \sa vfs_write_function
     */
    vfs_write_function write;
    
    /**
     * \brief Reads the target of a symlink.
     * \sa vfs_read_symlink_function
     */
    vfs_read_symlink_function read_symlink;
    
    /**
     * \brief Reads a directory entry from a directory.
     * \sa vfs_iter_function
     */
    vfs_iter_function iter;
    
    /**
     * \brief Finds a child under this directory by name.
     * \sa vfs_find_function
     */
    vfs_find_function find;
    
    /**
     * \brief Moves a node within this filesystem.
     * \sa vfs_move_function
     */
    vfs_move_function move;
    
    /**
     * \brief Hardlinks a node in a new location.
     * \sa vfs_link_function
     */
    vfs_link_function link;
    
    /**
     * \brief Removes a node from under a directory.
     * \sa vfs_unlink_function
     */
    vfs_unlink_function unlink;
} fs_ops;

/**
 * \brief Represents a type of filesystem.
 */
typedef struct fs_type
{
    const char* name; /**< \brief The name of the filesystem type. */
    const fs_ops* ops; /**< \brief The operations that can be done using this filesystem type. */
    
    /**
     * \brief Whether this filesystem type is a generic filesystem.
     * 
     * If true, then this filesystem is generic and can be mounted on a generic block device. If
     * false, then this filesystem is specific and cannot be mounted on a generic block device.
     */
    bool is_generic;
    
    struct fs_type* next; /**< \brief The next known filesystem type. */
} fs_type;

/**
 * \brief Represents a generic node in the virtual filesystem.
 * 
 * No assumptions should be made regarding how a virtual node is stored. A virtual node may be
 * stored on a local physical medium, in volatile memory, on a remote system, or even be
 * generated dynamically.
 * 
 * \warning When copying a reference to a node, always make sure to increment its reference count
 *          using \link vfs_node_ref \endlink and ensure that you call \link vfs_node_unref \endlink
 *          before discarding the reference. In addition, references to nodes that can be copied
 *          from another thread should always be placed behind a locking mechanism to ensure that
 *          the original reference is not discarded between when a copy of it is taken and when the
 *          node's reference count is incremented.
 */
typedef struct vfs_node
{
    /**
     * \brief The device to which this node belongs.
     * 
     * It is required that a thread hold the \link vfs_node::lock \endlink mutex while reading this
     * field and continue to hold it until such a time as the reference to the device is discarded.
     * 
     * \warning This may be set to NULL if this node has been orphaned. This could occur if the node
     *          is deleted or if the device it was on was forcibly disconnected. If this field is
     *          found to be NULL, the reference to this node should be discarded as soon as possible
     *          and \link vfs_node_unref \endlink called to allow the node to be released.
     */
    fs_device* dev;
    
    /**
     * \brief Operations which can be performed on this node.
     * 
     * It is required that a thread hold the \link vfs_node::lock \endlink mutex while reading this
     * field and continue to hold it until such a time as the reference to this structure is
     * discarded.
     * 
     * Any functions within this structure that cause the thread to block for I/O shall release this
     * mutex before blocking and re-acquire it once the operation has completed, whether or not the
     * operation was successful.
     * 
     * \warning This may be set to NULL if this node has been orphaned. This could occur if the node
     *          is deleted or if the device it was on was forcibly disconnected. If this field is
     *          found to be NULL, the reference to this node should be discarded as soon as possible
     *          and \link vfs_node_unref \endlink called to allow the node to be released.
     */
    const fs_ops* ops;
    
    /**
     * \brief A mutex protecting some key parts of this node.
     * 
     * If taking out this lock is required for a particular operation, then the documentation for
     * that specific member or function should make note that this mutex must be held.
     * 
     * No thread should hold more than one node lock at any given time, in order to prevent both
     * deadlocks as a result of the order in which the mutexes are acquired, as well as to prevent
     * starvation should an operation on one of the nodes block.
     */
    mutex lock;
    
    /**
     * \brief The inode number associated with this node.
     * 
     * This number is guaranteed to be unique for among all loaded nodes on a given filesystem. This
     * identifier shall not change while the node is loaded, but if this node is unloaded, it is
     * undefined whether the inode number will remain the same.
     */
    uint32 inode_no;
    
    /**
     * \brief The number of references that currently exist to this filesystem node.
     * 
     * This value should not be modified or read directly, except by internal filesystem code while
     * loading a new node, attempting to unload this node, or working with a node which is known to
     * have no external references. Instead, \link vfs_node_ref \endlink and
     * \link vfs_node_unref \endlink should be called to increment or decrement this reference count
     * atomically.
     */
    uint32 refcount;
    
    /**
     * \brief Flags that denote the current state of this node.
     * 
     * During the lifetime of this node, it is guaranteed that the portion of these flags returned
     * by \link VFS_TYPE \endlink will never change. For this reason, reading only these bits does
     * not require atomic operations, nor does it require that any lock be held.
     * 
     * Reading other flag bits requires that \link vfs_node::lock \endlink be held, as most of these
     * bits may be modified dynamically. These bits should only be modified by internal vfs and
     * filesystem code.
     */
    uint32 flags;
    
    /**
     * \brief The size of this node in bytes.
     * 
     * This field should only be considered to be valid if this node is a regualar file. For other
     * node types, the filesystem is free to use this size for any purpose.
     * 
     * Reading this value requires that \link vfs_node::lock \endlink be held.
     */
    uint64 size;
    
    /**
     * \brief Extra filesystem-specific information for this node.
     * 
     * This value should not be relied upon outside of filesystem-specific code.
     */
    void* extra;
    
    // Note: Doxygen chokes on this unnamed union, so we just tell doxygen to ignore the beginning
    //       and end so it sees the contained members as members of the struct, not the unnamed
    //       union.
    /// \cond
    union
    {
    /// \endcond
        /**
         * \brief The device which is currently mounted on this node.
         * 
         * This value is only valid if this node's \link vfs_node::flags \endlink has the
         * \link VFS_FLAG_MOUNTPOINT \endlink bit set.
         * 
         * Reading this value requires that \link vfs_node::lock \endlink be held.
         */
        struct fs_device* mounted;
    /// \cond
    };
    /// \endcond
} vfs_node;

/**
 * \brief Provides minimal information about an entry in a directory.
 * 
 * This structure only provides the bare minimum information that is required in order to be able to
 * identify an entry under a directory in the filesystem. Further information can be found by using
 * \link vfs_find_function \endlink with \link vfs_dirent::name \endlink to get the actual file
 * node's information. While it is possible to use \link vfs_load_function \endlink with the
 * provided \link vfs_dirent::inode_no \endlink to find the file information, it is not recommended
 * since not all filesystems support this function.
 */
typedef struct vfs_dirent
{
    char name[VFS_NAME_LENGTH + 1]; /**< \brief The null-terminated name of the entry. */
    uint32 inode_no; /**< \brief The inode number of the entry. */
} vfs_dirent;

/**
 * \brief The node representing the root of the virtual filesystem.
 * 
 * This node should only be used as a mountpoint for another filesystem. This node **does not**
 * support any of the typical node operations, and they will all fail with
 * \link E_NOT_SUPPORTED \endlink.
 */
extern vfs_node vfs_root;

/**
 * \brief The lock which protects the list of known filesystem types and the list of connected
 *        devices.
 * 
 * A thread **must not** hold any \link vfs_node::lock \endlink mutexes when attempting to acquire
 * this lock, or a deadlock could result.
 */
extern mutex vfs_list_lock;

/**
 * \brief The first known filesystem type.
 * 
 * This is the first in the linked list of known filesystem types. Further filesystem types can be
 * found by using \link fs_type::next \endlink until a NULL pointer is reached.
 * 
 * Traversing the list, modifying the list, and reading/writing any types obtained through traversal
 * of this list requires that the thread hold the \link vfs_list_lock \endlink mutex.
 */
extern fs_type* vfs_type_first;

/**
 * \brief The first known filesystem device.
 * 
 * This is the first in the linked list of connected filesystem devices. Further connected devices
 * can be found by using \link fs_device::next \endlink until a NULL pointer is reached.
 * 
 * Traversing the list, modifying the list, and reading/writing any device information obtained
 * through traversal of this list requires that the thread hold the \link vfs_list_lock \endlink
 * mutex.
 */
extern fs_device* vfs_device_first;

/**
 * \brief A memory pool to be used for the allocation of \link vfs_node \endlink structures.
 */
extern mempool_small vfs_node_pool;

/**
 * \brief A memory pool to be used for the allocation of \link fs_device \endlink structures.
 */
extern mempool_small vfs_device_pool;

/**
 * \brief A function which can be used in a \link fs_ops \endlink structure when a particular
 *        function is not supported.
 * 
 * This function simply ignores any arguments and returns \link E_NOT_SUPPORTED \endlink.
 */
extern uint32 vfs_null_op(void);

/// \cond
extern void vfs_init(const boot_param* param) __hidden;
/// \endcond

/**
 * \brief Registers a new filesystem type.
 * 
 * Calling this function requires that the calling thread hold the \link vfs_list_lock \endlink
 * mutex.
 * 
 * \param[in] type The type of the filesystem that is being registered.
 */
extern void vfs_register_fs(fs_type* type);

/**
 * \brief Unregisters a filesystem type.
 * 
 * Calling this function requires that the calling thread hold the \link vfs_list_lock \endlink
 * mutex.
 * 
 * \param[in] type The type of the filesystem that is being unregistered.
 */
extern void vfs_unregister_fs(fs_type* type);

/**
 * \brief Creates a new device with the given characteristics.
 * 
 * Calling this function requires that the calling thread hold the \link vfs_list_lock \endlink
 * mutex.
 * 
 * \param[in] name The name that the device will be given.
 * 
 * \param[in] ops A structure containing function pointers to perform given tasks on this device.
 * 
 * \param[in] sector_size The size of each sector on this device.
 * 
 * \param[in] num_sectors The number of sectors that are present on this device.
 * 
 * \param[in] dev_extra A pointer to extra device-specific information that will be placed in the
 *                      \link fs_device::dev_extra \endlink field.
 * 
 * \return The newly created filesystem device.
 */
extern fs_device* vfs_create_device(const char* name, const fs_device_ops* ops, uint32 sector_size, uint64 num_sectors, void* dev_extra) __warn_unused_result;

/**
 * \brief Destroys an existing device.
 * 
 * This function should only be used when a device is being disconnected, as the filesystem will be
 * **forcibly unloaded**, even if an error occurs while cleaning up the filesystem information. If
 * a more gentle approach is needed, use \link vfs_unmount \endlink instead to unmount the
 * filesystem, leaving the device itself intact.
 * 
 * Calling this function requires that the calling thread hold the \link vfs_list_lock \endlink
 * mutex.
 * 
 * This function should **only** be called from device-specific code, as it is the responsibility
 * of the caller to clean up any extra device memory allocated to the device. It is the
 * responsibility of the caller to take a copy of \link fs_device::dev_extra \endlink before calling
 * this function and to free any extra device-specific memory allocations made for this device.
 * Device-specific information **must not** be freed before calling this function as it could be
 * in use for servicing any existing requests.
 * 
 * It is also the responsibility of the device and filesystem drivers to ensure that any existing
 * requests in process are interrupted cleanly and do not crash the system. If interrupted, such
 * operations should generally return \link E_IO_ERROR \endlink.
 * 
 * \param[in] dev The device which is being destroyed. Once this function has returned, this device
 *                pointer is **no longer valid** and may point to another device entirely or to
 *                unallocated memory.
 */
extern void vfs_destroy_device(fs_device* dev);

/**
 * \brief Normalizes a path to remove any uses of . or .. directories and make the path absolute.
 * 
 * Any uses of . or .. directories in the given path will be removed from the path and the necessary
 * operations performed on the output pathname. If the path given is relative, it will be made
 * absolute with relation to the current path given.
 * 
 * This function does not do any filesystem lookups, including resolving symlinks and verifying the
 * existence of the specified path.
 * 
 * \param[in] cur_path The working directory in the filesystem that any relative paths should be
 *                     relative to. This path must be absolute and should end in a directory
 *                     separator as specified by \link VFS_SEPARATOR_CHAR \endlink.
 * 
 * \param[in] path_in The path that is going to be normalized.
 * 
 * \param[out] path_out The buffer in which the final, normalized path should be placed. It is
 *                      acceptable for this to point to the same buffer as cur_path, but not for it
 *                      to overlap with cur_path if they are not equal. This buffer must be
 *                      \link VFS_PATH_LENGTH \endlink + 1 bytes in length.
 * 
 * \param[out] len If this is not NULL, then this will be set to the length of the final path.
 * 
 * \retval E_SUCCESS The pathname was successfully normalized.
 * \retval E_NAME_TOO_LONG Either one of the directory/file names along the path is too long or the
 *                         path (or an intermediate path) is too long.
 */
extern uint32 vfs_normalize_path(const char* cur_path, const char* path_in, char* path_out, size_t* len) __warn_unused_result;

/**
 * \brief Resolves a path to find the actual node that it refers to.
 * 
 * If the final node is requested, it will have its reference count automatically incremented. It
 * is the responsibility of the caller to call \link vfs_node_unref \endlink when it is done with
 * this reference.
 * 
 * \param[in] root The node that should be considered to be the root of the filesystem. This node
 *                 must be fully resolved. Mountpoints and symlinks will not be followed from the
 *                 root node.
 * 
 * \param[in] cur_path The working directory in the filesystem that the path will be relative to.
 *                     This path must be absolute and should end in a directory separator as
 *                     specified by \link VFS_SEPARATOR_CHAR \endlink.
 * 
 * \param[in] path The path that is being resolved. If the path ends in a separator character as
 *                 specified by \link VFS_SEPARATOR_CHAR \endlink, then the final node along the
 *                 path must be a directory and will be fully resolved.
 * 
 * \param[in] follow_symlinks If true, symlinks along the path will be followed. If false, any
 *                            symlinks will not be followed and an \link E_NOT_DIR \endlink will
 *                            result.
 * 
 * \param[out] node_out If non-NULL, this will be set to point to the found node.
 * 
 * \param[out] canonical_path_out If non-NULL, then this buffer will be filled with the final
 *                                canonical path, with any symlinks resolved. This buffer must be
 *                                at least \link VFS_PATH_LENGTH \endlink + 1 bytes long.
 * 
 * \retval E_SUCCESS The path was successfully resolved.
 * \retval E_NOT_FOUND One or more of the nodes along the path could not be found.
 * \retval E_IO_ERROR An I/O error was encountered while attempting to resolve the path.
 * \retval E_NOT_DIR One of the nodes along the path was expected to be a directory, but it is not.
 * \retval E_NAME_TOO_LONG One of the names along the path was too long or the path (or an
 *                         intermediate path) is too long.
 * \retval E_LOOP There are more than \link VFS_SYMLINK_DEPTH \endlink symlinks along the path.
 * \retval E_NO_MEMORY There is insufficient memory to read the device in order to resolve the path.
 */
extern uint32 vfs_resolve_path(vfs_node* root, const char* cur_path, const char* path, bool follow_symlinks, vfs_node** node_out, char* canonical_path_out);

/**
 * \brief Atomically increments the reference count of a node.
 * 
 * If the reference count of the requested node is 0, then the system will crash as a result. This
 * is usually a sign of a race condition, since a node may be removed from the cache at any time if
 * its reference count is 0. If it is normal for the reference count to potentially be 0, (i.e. the
 * node is being read from a cache before it was disposed) then this should be detected and handled
 * externally.
 * 
 * \warning When copying a reference which may be discarded by another thread, it is important to
 *          protect that reference with a lock which prevents it from being discarded during the
 *          copy process. When this is necessary, this function must **always** be called **before**
 *          the relevant lock has been released.
 * 
 * \param[in] node The node which is being referenced.
 */
extern void vfs_node_ref(vfs_node* node);

/**
 * \brief Atomically decrements the reference count of a node.
 * 
 * If the reference count of the requested node is 0, then the system will crash as a result. This
 * is **always** a sign of a bug, since the reference count should **not** be decremented unless it
 * was previously incremented.
 * 
 * If the referent count of the requested node becomes 0 as a result, then the node will be unloaded
 * via \link vfs_unload_function \endlink.
 * 
 * The calling thread **must not** hold the \link vfs_node::lock \endlink mutex for the given node.
 * If the node were to be unloaded, an attempt to release this mutex would result in an access to
 * unallocated memory.
 * 
 * \param[in] node The node for which a reference is being discarded.
 */
extern void vfs_node_unref(vfs_node* node);

/**
 * \brief Mounts a filesystem device at the requested mountpoint.
 * 
 * If the supplied device has not yet been read by a filesystem, the system will attempt to find
 * a filesystem which can be used to read the device automatically.
 * 
 * The calling thread **must not** currently be holding any \link vfs_node::lock \endlink mutexes
 * when calling this function. The calling thread must hold the \link vfs_list_lock \endlink mutex,
 * and must not have released it since obtaining the device pointer.
 * 
 * \param[in] mountpoint The node that the device should be mounted on.
 * 
 * \param[in] dev The device which should be mounted.
 * 
 * \retval E_SUCCESS The device was successfully mounted at the given mountpoint.
 * \retval E_IO_ERROR There was an I/O error while attempting to determine the filesystem on the
 *                    requested device.
 * \retval E_INVALID No filesystem driver in the system could mount the requested device.
 * \retval E_BUSY The requested device was already mounted.
 * \retval E_NOT_DIR The mountpoint is not a directory.
 * \retval E_NO_MEMORY There is insufficient memory to determine the filesystem on the requested
 *                     device.
 * \retval E_ALREADY_EXISTS There is already a device mounted at the mountpoint.
 */
extern uint32 vfs_mount(vfs_node* mountpoint, fs_device* dev) __warn_unused_result;

/**
 * \brief Unmounts a filesystem from the requested mountpoint.
 * 
 * This operation will not result in the destruction of the device, that must be accomplished
 * separately, through a device-specific function.
 * 
 * The calling thread **must** be currently holding the \link vfs_list_lock \endlink mutex and
 * **must not** currently be holding any \link vfs_node::lock \endlink mutexes.
 * 
 * \param[in] mountpoint The directory at which the device to be unmounted is currently mounted.
 * 
 * \param[in] force If 0, then the unmount will only proceed if it can be done cleanly. If 1, then
 *                  in the event of an error, the root node will be disconnected from its
 *                  mountpoint, but the filesystem will not fully unmount and the proper error code
 *                  will be returned. If 2, then the filesystem will be completely destroyed in the
 *                  event of an error and that error will not be returned.
 * 
 * \retval E_SUCCESS The device at the given mountpoint was successfully unmounted.
 * \retval E_IO_ERROR An I/O error was encountered while unmounting the filesystem and the unmount
 *                    is not being forced.
 * \retval E_INVALID No device is currently mounted at the given mountpoint.
 * \retval E_BUSY One or more files are currently open on this device and the unmount is not being
 *                forced.
 * \retval E_NO_MEMORY There is insufficient memory to cleanly unmount the filesystem and the
 *                     unmount is not being forced.
 */
extern uint32 vfs_unmount(vfs_node* mountpoint, uint32 force);

#endif
