#include <fs/initrd.h>
#include <string.h>

#include <core/klog.h>

#include <memory/virt.h>

static uint32 _initrd_read(struct vfs_device* dev, uint64 sector, size_t num_sectors, uint8* buffer)
{
    if (sector + num_sectors < sector || sector + num_sectors > dev->num_sectors)
        return E_INVALID;

    for (uint64 s = sector; s != sector + num_sectors; s++)
    {
        uint8* sbe = (uint8*)((uint32)dev->dev_extra + (uint32)(sector + 1) * dev->sector_size);

        for (uint8* sb = (uint8*)((uint32)dev->dev_extra + (uint32)sector * dev->sector_size); sb != sbe; sb++)
            *(buffer++) = *sb;
    }

    return E_SUCCESS;
}

static uint32 _initrd_write(struct vfs_device* dev, uint64 sector, size_t num_sectors, const uint8* buffer)
{
    return E_IO_ERROR;
}

static const vfs_device_ops initrd_ops = {
    .read = _initrd_read,
    .write = _initrd_write
};

static uint32 _initrd_map(const boot_param_module_info* mod, void** map_addr, uint32* num_pages)
{
    *num_pages = (uint32)((mod->end_address - mod->start_address + FRAME_SIZE - 1) / FRAME_SIZE);

    *map_addr = kmem_virt_alloc(*num_pages);
    if (*map_addr == NULL)
        return E_NO_MEMORY;

    for (uint32 i = 0; i < *num_pages; i++)
    {
        addr_v va = (addr_v)*map_addr + i * FRAME_SIZE;
        addr_p pa = (addr_p)mod->start_address + i * FRAME_SIZE;

        if (!kmem_page_global_map(va, PT_ENTRY_GLOBAL | PT_ENTRY_NO_EXECUTE, false, pa))
        {
            for (uint32 j = 0; j < i; j++)
            {
                va = (addr_v)*map_addr + j * FRAME_SIZE;
                pa = (addr_p)mod->start_address + i * FRAME_SIZE;

                kmem_page_global_unmap(va, false);
            }

            return E_NO_MEMORY;
        }
    }

    return E_SUCCESS;
}

static uint32 _initrd_load(const boot_param_module_info* mod, vfs_device** dev)
{
    uint32 num_pages;
    void* map_addr;

    klog(KLOG_LEVEL_DEBUG, "Loading initrd from %s...\n", mod->name);

    _initrd_map(mod, &map_addr, &num_pages);
    *dev = vfs_device_create("initrd", &initrd_ops, INITRD_SECTOR_SIZE, num_pages * FRAME_SIZE / INITRD_SECTOR_SIZE, map_addr);

    klog(KLOG_LEVEL_DEBUG, "Loaded %dKiB from initrd\n", num_pages * FRAME_SIZE / 1024);
    return E_SUCCESS;
}

uint32 initrd_create(const boot_param* param, vfs_device** dev)
{
    const boot_param_module_info* mod = boot_param_find_module(param, cmdline_get_str(param, "initrd", "/boot/initrd"));

    if (mod == NULL)
        return E_NOT_FOUND;

    return _initrd_load(mod, dev);
}
