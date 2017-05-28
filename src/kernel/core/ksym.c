#include <core/ksym.h>
#include <core/klog.h>
#include <core/elf.h>

#include <memory/early.h>
#include <string.h>

extern const void _ld_kernel_end;

static uint32 num_kernel_symbols = 0;
static kernel_symbol* kernel_symbols = NULL;

static bool is_symbol_match(const kernel_symbol* ksym, uint32 virtual_address, uint32 flags)
{
    if ((flags & KSYM_ALOOKUP_RET) == KSYM_ALOOKUP_RET)
    {
        return (virtual_address > ksym->address && virtual_address <= (ksym->address + ksym->size));
    }
    else
    {
        return (virtual_address >= ksym->address && virtual_address < (ksym->address + ksym->size));
    }
}

const kernel_symbol* ksym_address_lookup(uint32 virtual_address, uint32* symbol_offset, uint32 flags)
{
    kernel_symbol* ksym = kernel_symbols;
    kernel_symbol* ksym_end = kernel_symbols + num_kernel_symbols;
    
    while (ksym != ksym_end)
    {
        if (is_symbol_match(ksym, virtual_address, flags))
        {
            if (symbol_offset != NULL)
                *symbol_offset = virtual_address - ksym->address;
            
            return ksym;
        }
        
        ksym++;
    }
    
    return NULL;
}

static void load_kernel_symtab(elf32_shdr* symtab, elf32_shdr* strtab)
{
    elf32_sym* sym_entry;
    elf32_sym* sym_end;
    const char* sym_name;
    
    kernel_symbol* ksym;
    char* ksym_name;
    
    num_kernel_symbols = 0;
    
    sym_entry = (elf32_sym*) (symtab->addr + 0xC0000000);
    sym_end = (elf32_sym*) ((uint32)sym_entry + ((symtab->size / symtab->entsize) * symtab->entsize));
    
    while (sym_entry != sym_end)
    {
        if (sym_entry->value != 0 && ELF32_ST_TYPE(sym_entry->info) != STT_SECTION)
            num_kernel_symbols++;
        
        sym_entry = (elf32_sym*) ((uint32) sym_entry + symtab->entsize);
    }
    
    kernel_symbols = ksym = kmalloc_early(sizeof(kernel_symbol) * num_kernel_symbols, __alignof__(kernel_symbol), NULL);
    sym_entry = (elf32_sym*) (symtab->addr + 0xC0000000);
    
    while (sym_entry != sym_end)
    {
        if (sym_entry->value != 0 && ELF32_ST_TYPE(sym_entry->info) != STT_SECTION)
        {
            sym_name = (const char*) (strtab->addr + 0xC0000000 + sym_entry->name);
            ksym->name = ksym_name = kmalloc_early(strlen(sym_name) + 1, 1, NULL);
            
            strcpy(ksym_name, sym_name);
            ksym->address = sym_entry->value;
            ksym->size = sym_entry->size;
            
            switch (ELF32_ST_TYPE(sym_entry->info))
            {
                case STT_OBJECT:
                    ksym->type = KSYM_TYPE_OBJECT;
                    break;
                case STT_FUNC:
                    ksym->type = KSYM_TYPE_FUNCTION;
                    break;
                default:
                    ksym->type = KSYM_TYPE_OTHER;
                    break;
            }
            
            if (ELF32_ST_BIND(sym_entry->info) == STB_LOCAL)
            {
                ksym->visibility = KSYM_VISIBILITY_FILE_LOCAL;
            }
            else
            {
                switch (ELF32_ST_VISIBILITY(sym_entry->other))
                {
                    case STV_INTERNAL:
                    case STV_HIDDEN:
                    case STV_PROTECTED:
                        ksym->visibility = KSYM_VISIBILITY_PRIVATE;
                        break;
                    default:
                        ksym->visibility = KSYM_VISIBILITY_PUBLIC;
                        break;
                }
            }
            
            ksym++;
        }
        
        sym_entry = (elf32_sym*) ((uint32) sym_entry + symtab->entsize);
    }
}

void ksym_load_kernel_symbols(multiboot_info* multiboot)
{
    elf32_shdr* shdr_entry;
    elf32_shdr* shdr_end;
    elf32_shdr* shdr_strtab;
    
    if (multiboot->flags & MB_FLAG_ELF_SHDR)
    {
        shdr_entry = (elf32_shdr*) (multiboot->elf_shdr_table.addr + 0xC0000000);
        shdr_end = (elf32_shdr*) ((uint32) shdr_entry + (multiboot->elf_shdr_table.num * multiboot->elf_shdr_table.size));
        
        while (shdr_entry != shdr_end)
        {
            if (shdr_entry->type == SHT_SYMTAB && shdr_entry->addr != 0)
            {
                shdr_strtab = (elf32_shdr*) (multiboot->elf_shdr_table.addr + 0xC0000000 + (shdr_entry->link * multiboot->elf_shdr_table.size));
                load_kernel_symtab(shdr_entry, shdr_strtab);
                return;
            }
            
            shdr_entry = (elf32_shdr*) ((uint32) shdr_entry + multiboot->elf_shdr_table.size);
        }
    }
    
    klog(KLOG_LEVEL_WARN, "No kernel symbol table found!\n");
}
