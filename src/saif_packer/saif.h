#ifndef SAIF_H
#define SAIF_H

#include <stdint.h>

#define SAIF_MAGIC ((char[]) { 'S', 'A', 'I', 'F' })
#define SAIF_BLOCK_SIZE 64
#define SAIF_REVISION 0

#define SAIF_NAME_MAX 50

#define SAIF_TYPE_FILE 0
#define SAIF_TYPE_DIR 1

typedef char saif_block[SAIF_BLOCK_SIZE];

typedef struct
{
    char magic[sizeof(SAIF_MAGIC)];
    uint32_t revision;
    uint32_t header_checksum;
    
    uint32_t root_dir_block;
    uint32_t root_dir_length;
    uint32_t root_dir_checksum;
} saif_superblock;

typedef struct
{
    char name[SAIF_NAME_MAX + 1];
    uint8_t type;
    
    uint32_t block;
    uint32_t length;
    uint32_t checksum;
} saif_dirent;

static inline uint32_t saif_calculate_checksum(uint32_t* b, size_t s)
{
    uint32_t checksum = 0;
    
    s += sizeof(uint32_t) - 1;
    s /= sizeof(uint32_t);
    
    while (s--)
        checksum += *(b++);
    
    return -checksum;
}

#endif
