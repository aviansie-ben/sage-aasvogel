#include <core/bootparam.h>
#include <memory/early.h>

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

void parse_boot_cmdline(multiboot_info* multiboot, boot_param* param)
{
    char* cmdline = (char*)(multiboot->cmdline_addr + 0xC0000000);
    
    param->multiboot = multiboot;
    
    if ((multiboot->flags & MB_FLAG_CMDLINE) == MB_FLAG_CMDLINE)
    {
        param->num_cmdline_parts = cmdline_preparse(cmdline);
        param->cmdline_parts = cmdline_parse(cmdline, param->num_cmdline_parts);
    }
    else
    {
        param->num_cmdline_parts = 0;
        param->cmdline_parts = NULL;
    }
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
