#ifndef CORE_ELF_H
#define CORE_ELF_H

#include <typedef.h>

#define EI_NIDENT 16
#define EI_MAG0    0
#define EI_MAG1    1
#define EI_MAG2    2
#define EI_MAG3    3
#define EI_CLASS   4
#define EI_DATA    5
#define EI_VERSION 6

#define EI_MAG0_VAL 0x7fu
#define EI_MAG1_VAL 'E'
#define EI_MAG2_VAL 'L'
#define EI_MAG3_VAL 'F'

#define ELFCLASSNONE 0
#define ELFCLASS32   1
#define ELFCLASS64   2

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE 0
#define ET_REL  1
#define ET_EXEC 2
#define ET_DYN  3
#define ET_CORE 4

#define EM_NONE  0
#define EM_386   3

#define EV_NONE    0
#define EV_CURRENT 1

#define SHN_UNDEF     0x0000
#define SHN_LORESERVE 0xff00
#define SHN_ABS       0xfff1
#define SHN_COMMON    0xfff2
#define SHN_HIRESERVE 0xffff

#define SHT_NULL      0
#define SHT_PROGBITS  1
#define SHT_SYMTAB    2
#define SHT_STRTAB    3
#define SHT_RELA      4
#define SHT_HASH      5
#define SHT_DYNAMIC   6
#define SHT_NOTE      7
#define SHT_NOBITS    8
#define SHT_REL       9
#define SHT_SHLIB    10
#define SHT_DYNSYM   11

#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

#define STN_UNDEF 0

#define ELF32_ST_BIND(i)   ((i) >> 4)
#define ELF32_ST_TYPE(i)   ((i) & 0xf)
#define ELF32_ST_INFO(b,t) (((b) << 4) + ((t) & 0xf))

#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

#define STV_DEFAULT   0
#define STV_INTERNAL  1
#define STV_HIDDEN    2
#define STV_PROTECTED 3
#define STV_BITMASK   3

#define ELF32_R_SYM(i)    ((i) >> 8)
#define ELF32_R_TYPE(i)   ((unsigned char)(i))
#define ELF32_R_INFO(s,t) (((s) << 8) + (unsigned char)(t))

#define R_386_NONE      0
#define R_386_32        1
#define R_386_PC32      2
#define R_386_GOT32     3
#define R_386_PLT32     4
#define R_386_COPY      5
#define R_386_GLOB_DAT  6
#define R_386_JMP_SLOT  7
#define R_386_RELATIVE  8
#define R_386_GOTOFF    9
#define R_386_GOTPC    10

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

#define DT_NULL      0
#define DT_NEEDED    1
#define DT_PLTRELSZ  2
#define DT_PLTGOT    3
#define DT_HASH      4
#define DT_STRTAB    5
#define DT_SYMTAB    6
#define DT_RELA      7
#define DT_RELASZ    8
#define DT_RELAENT   9
#define DT_STRSZ    10
#define DT_SYMENT   11
#define DT_INIT     12
#define DT_FINI     13
#define DT_SONAME   14
#define DT_RPATH    15
#define DT_SYMBOLIC 16
#define DT_REL      17
#define DT_RELSZ    18
#define DT_RELENT   19
#define DT_PLTREL   20
#define DT_DEBUG    21
#define DT_TEXTREL  22
#define DT_JMPREL   23

typedef uint32 elf32_addr;
typedef uint16 elf32_half;
typedef uint32 elf32_off;
typedef int32  elf32_sword;
typedef uint32 elf32_word;

typedef struct {
    unsigned char ident[EI_NIDENT];
    elf32_half    type;
    elf32_half    machine;
    elf32_word    version;
    elf32_addr    entry;
    elf32_off     phoff;
    elf32_off     shoff;
    elf32_word    flags;
    elf32_half    ehsize;
    elf32_half    phentsize;
    elf32_half    phnum;
    elf32_half    shentsize;
    elf32_half    shnum;
    elf32_half    shstrndx;
} elf32_ehdr;

typedef struct {
    elf32_word name;
    elf32_word type;
    elf32_word flags;
    elf32_addr addr;
    elf32_off  offset;
    elf32_word size;
    elf32_word link;
    elf32_word info;
    elf32_word addralign;
    elf32_word entsize;
} elf32_shdr;

typedef struct {
    elf32_word    name;
    elf32_addr    value;
    elf32_word    size;
    unsigned char info;
    unsigned char other;
    elf32_half    shndx;
} elf32_sym;

typedef struct {
    elf32_addr offset;
    elf32_word info;
} elf32_rel;

typedef struct {
    elf32_addr  offset;
    elf32_word  info;
    elf32_sword addend;
} elf32_rela;

typedef struct {
    elf32_word type;
    elf32_off  offset;
    elf32_addr vaddr;
    elf32_addr paddr;
    elf32_word filesz;
    elf32_word memsz;
    elf32_word flags;
    elf32_word align;
} elf32_phdr;

typedef struct {
    elf32_sword tag;
    union {
        elf32_word val;
        elf32_addr ptr;
    };
} elf32_dyn;

#endif
