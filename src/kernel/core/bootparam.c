#include <core/bootparam.h>
#include <memory/early.h>
#include <string.h>

static uint32 cmdline_preparse(char* cmdline)
{
    size_t l = 0, n = 0;
    
    while (true)
    {
        while (*cmdline != ' ' && *cmdline != '\0')
        {
            cmdline++;
            l++;
        }
        
        if (l != 0)
        {
            l = 0;
            n++;
        }
        
        if (*cmdline == '\0')
        {
            // The first part of the command line is ignored, since it is just
            // the location of the kernel binary.
            return (n == 0) ? 0 : (n - 1);
        }
        else
        {
            *cmdline = '\0';
            cmdline++;
        }
    }
}

static const char** cmdline_parse(char* cmdline, size_t n)
{
    size_t i;
    const char** parts;
    
    if (n == 0)
        return NULL;
    
    parts = kmalloc_early(n * sizeof(const char*), __alignof__(const char*), NULL);
    
    for (i = 0; i < n; i++)
    {
        while (*cmdline != '\0')
            cmdline++;
        
        while (*cmdline == '\0')
            cmdline++;
        
        parts[i] = cmdline;
    }
    
    return parts;
}

static void parse_boot_cmdline(boot_param* param, const multiboot_info* multiboot)
{
    if ((multiboot->flags & MB_FLAG_CMDLINE) == MB_FLAG_CMDLINE)
    {
        char* cmdline = (char*)(multiboot->cmdline_addr + 0xC0000000);
        
        param->num_cmdline_parts = cmdline_preparse(cmdline);
        param->cmdline_parts = cmdline_parse(cmdline, param->num_cmdline_parts);
    }
    else
    {
        param->num_cmdline_parts = 0;
        param->cmdline_parts = NULL;
    }
}

static void copy_module_info(boot_param_module_info* i, const multiboot_module_entry* e)
{
    if (e->name != 0)
    {
        const char* name = (const char*)(e->name + 0xC0000000);
        
        i->name = kmalloc_early(strlen(name) + 1, __alignof__(char), NULL);
        strcpy((char*) i->name, name);
    }
    else
    {
        i->name = "";
    }
    
    i->start_address = e->mod_start;
    i->end_address = e->mod_end;
}

static void copy_boot_modules(boot_param* param, const multiboot_info* multiboot)
{
    if ((multiboot->flags & MB_FLAG_MODULES) == MB_FLAG_MODULES && multiboot->mods_count != 0)
    {
        const multiboot_module_entry* e = (const multiboot_module_entry*)(multiboot->mods_addr + 0xC0000000);
        
        param->num_modules = multiboot->mods_count;
        param->modules = kmalloc_early(multiboot->mods_count * sizeof(boot_param_module_info), __alignof__(boot_param_module_info), NULL);
        
        for (size_t i = 0; i < multiboot->mods_count; i++)
        {
            copy_module_info((boot_param_module_info*) &param->modules[i], e);
            e++;
        }
    }
    else
    {
        param->num_modules = 0;
        param->modules = NULL;
    }
}

static void copy_mmap_region(boot_param_mmap_region* r, const multiboot_mmap_entry* e)
{
    r->type = e->type;
    r->start_address = e->base_addr;
    r->end_address = e->base_addr + e->length;
}

static const multiboot_mmap_entry* next_mmap_entry(const multiboot_mmap_entry* e)
{
    return (const multiboot_mmap_entry*)((uint32)e + e->size + sizeof(e->size));
}

static size_t count_mmap_regions(const multiboot_mmap_entry* e, size_t size)
{
    const multiboot_mmap_entry* end = (const multiboot_mmap_entry*)((uint32)e + size);
    size_t i = 0;
    
    while (e != end)
    {
        e = next_mmap_entry(e);
        i++;
    }
    
    return i;
}

static void copy_mmap_regions(boot_param* param, const multiboot_info* multiboot)
{
    if ((multiboot->flags & MB_FLAG_MEM_MAP) == MB_FLAG_MEM_MAP)
    {
        const multiboot_mmap_entry* e = (const multiboot_mmap_entry*)(multiboot->mmap_addr + 0xC0000000);
        
        param->num_mmap_regions = count_mmap_regions(e, multiboot->mmap_length);
        param->mmap_regions = kmalloc_early(param->num_mmap_regions * sizeof(boot_param_mmap_region), __alignof__(boot_param_mmap_region), NULL);
        
        for (size_t i = 0; i < param->num_mmap_regions; i++)
        {
            copy_mmap_region((boot_param_mmap_region*) &param->mmap_regions[i], e);
            e = next_mmap_entry(e);
        }
    }
    else
    {
        param->num_mmap_regions = 0;
        param->mmap_regions = NULL;
    }
}

void boot_param_init(boot_param* param, const multiboot_info* multiboot)
{
    parse_boot_cmdline(param, multiboot);
    copy_boot_modules(param, multiboot);
    copy_mmap_regions(param, multiboot);
}

bool cmdline_get_bool(const boot_param* param, const char* param_name)
{
    size_t i;
    const char* part;
    const char* pn_part;
    
    for (i = 0; i < param->num_cmdline_parts; i++)
    {
        part = param->cmdline_parts[i];
        
        for (pn_part = param_name; *pn_part != '\0' && *pn_part == *part; pn_part++, part++) ;
        
        if (*pn_part == '\0' && *part == '\0')
            return true;
    }
    
    return false;
}

const char* cmdline_get_str(const boot_param* param, const char* param_name, const char* def_value)
{
    size_t i;
    const char* part;
    const char* pn_part;
    
    for (i = 0; i < param->num_cmdline_parts; i++)
    {
        part = param->cmdline_parts[i];
        
        for (pn_part = param_name; *pn_part != '\0' && *pn_part == *part; pn_part++, part++) ;
        
        if (*pn_part == '\0' && *part == '=')
            return part + 1;
    }
    
    return def_value;
}

int32 cmdline_get_int(const boot_param* param, const char* param_name, int32 min_value, int32 max_value, int32 def_value)
{
    const char* str_value = cmdline_get_str(param, param_name, NULL);
    
    if (str_value == NULL) return def_value;
    
    bool error;
    int32 int_value = (int32) strtonum(str_value, min_value, max_value, &error);
    
    return error ? def_value : int_value;
}
