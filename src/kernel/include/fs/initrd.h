#ifndef FS_INITRD_H
#define FS_INITRD_H

#include <typedef.h>
#include <core/bootparam.h>

#include <fs/vfs.h>

#define INITRD_SECTOR_SIZE 64

extern uint32 initrd_create(const boot_param* param, fs_device** dev) __warn_unused_result __hidden;
extern void initrd_destroy(fs_device* dev) __hidden;

#endif
