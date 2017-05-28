#ifndef PACK_H
#define PACK_H

#include <stdint.h>
#include "saif.h"

typedef struct file_to_pack
{
    char name[SAIF_NAME_MAX + 1];
    uint32_t selected_block;
    
    const char* path;
    uint32_t length;
} file_to_pack;

typedef struct dir_to_pack
{
    char name[SAIF_NAME_MAX + 1];
    uint32_t selected_block;
    
    const char* path;
    
    uint32_t       files_length;
    uint32_t       files_capacity;
    file_to_pack** files;
    
    uint32_t             dirs_length;
    uint32_t             dirs_capacity;
    struct dir_to_pack** dirs;
} dir_to_pack;

dir_to_pack* saif_prepack(const char* root_dir);
dir_to_pack* saif_pack(dir_to_pack* root_dir, uint32_t* end_block);
int saif_write(dir_to_pack* root_dir, const char* output_file, uint32_t end_block);

#endif
