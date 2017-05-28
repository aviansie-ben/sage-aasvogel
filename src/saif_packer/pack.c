#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

#include "pack.h"
#include "saif.h"

static void** extend_array(uint32_t* length, uint32_t* capacity, void*** arr)
{
    if (*arr == NULL)
    {
        *length = 1;
        *capacity = 10;
        
        *arr = malloc(sizeof(**arr) * 10);
    }
    else if (*length == *capacity)
    {
        (*length)++;
        *capacity *= 2;
        
        *arr = realloc(*arr, sizeof(**arr) * (*capacity));
    }
    else
    {
        (*length)++;
    }
    
    return &(*arr)[*length - 1];
}

static char* construct_path(const char* parent_path, const char* name)
{
    char* buf = malloc(strlen(parent_path) + 1 + strlen(name) + 1);
    sprintf(buf, "%s/%s", parent_path, name);
    
    return buf;
}

static void free_file_to_pack(file_to_pack* f)
{
    free((void*)f->path);
    free(f);
}

static void free_dir_to_pack(dir_to_pack* d)
{
    for (uint32_t i = 0; i < d->dirs_length; i++)
        free_dir_to_pack(d->dirs[i]);
    
    for (uint32_t i = 0; i < d->files_length; i++)
        free_file_to_pack(d->files[i]);
    
    free((void*)d->path);
    free(d->dirs);
    free(d->files);
}

static file_to_pack* prepack_file(const char* path, const char* name, const struct stat* s)
{
    if (strlen(name) > SAIF_NAME_MAX)
    {
        printf("Name of file %s is too long\n", path);
        return NULL;
    }
    
    file_to_pack* f = malloc(sizeof(file_to_pack));
    
    strcpy(f->name, name);
    
    f->path = malloc(strlen(path) + 1);
    strcpy((char*) f->path, path);
    
    f->length = s->st_size;
    
    return f;
}

static dir_to_pack* prepack_dir(const char* path, const char* name)
{
    DIR* dp;
    
    if (strlen(name) > SAIF_NAME_MAX)
    {
        printf("Name of directory %s is too long\n", path);
        return NULL;
    }
    
    if (!(dp = opendir(path)))
    {
        printf("Failed to read directory %s\n", path);
        return NULL;
    }
    
    dir_to_pack* d = malloc(sizeof(dir_to_pack));
    struct dirent* de;
    
    strcpy(d->name, name);
    
    d->path = malloc(strlen(path) + 1);
    strcpy((char*) d->path, path);
    
    d->files_length = d->files_capacity = 0;
    d->files = NULL;
    
    d->dirs_length = d->dirs_capacity = 0;
    d->dirs = NULL;
    
    while ((de = readdir(dp)) != NULL)
    {
        // Always ignore the . and .. directories, as trying to pack them will cause infinite
        // recursion.
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        
        char* child_path = construct_path(path, de->d_name);
        struct stat s;
        
        if (lstat(child_path, &s))
        {
            printf("Failed to lstat path %s\n", child_path);
            
            free(child_path);
            free_dir_to_pack(d);
            closedir(dp);
            return NULL;
        }
        
        if (S_ISREG(s.st_mode))
        {
            if ((*extend_array(&d->files_length, &d->files_capacity, (void***) &d->files) = prepack_file(child_path, de->d_name, &s)) == NULL)
            {
                free(child_path);
                free_dir_to_pack(d);
                closedir(dp);
                return NULL;
            }
        }
        else if (S_ISDIR(s.st_mode))
        {
            if ((*extend_array(&d->dirs_length, &d->dirs_capacity, (void***) &d->dirs) = prepack_dir(child_path, de->d_name)) == NULL)
            {
                free(child_path);
                free_dir_to_pack(d);
                closedir(dp);
                return NULL;
            }
        }
        
        free(child_path);
    }
    
    return d;
}

dir_to_pack* saif_prepack(const char* root_dir)
{
    char* path;
    
    if ((path = realpath(root_dir, NULL)) == NULL)
    {
        printf("Failed to read directory %s\n", path);
        return NULL;
    }
    
    dir_to_pack* p = prepack_dir(path, "");
    
    free(path);
    return p;
}

static int pack_file(file_to_pack* f, uint32_t* next_block)
{
    printf("0x%08X     F  %s\n", *next_block, f->path);
    
    f->selected_block = *next_block;
    *next_block += (f->length + SAIF_BLOCK_SIZE - 1) / SAIF_BLOCK_SIZE;
    
    return 0;
}

static int pack_dir(dir_to_pack* d, uint32_t* next_block)
{
    printf("0x%08X     D  %s\n", *next_block, d->path);
    
    d->selected_block = *next_block;
    *next_block += ((d->files_length + d->dirs_length) * sizeof(saif_dirent) + SAIF_BLOCK_SIZE - 1) / SAIF_BLOCK_SIZE;
    
    for (uint32_t i = 0; i < d->files_length; i++)
        pack_file(d->files[i], next_block);
    
    for (uint32_t i = 0; i < d->dirs_length; i++)
        pack_dir(d->dirs[i], next_block);
    
    return 0;
}

dir_to_pack* saif_pack(dir_to_pack* root_dir, uint32_t* end_block)
{
    printf("Block       Type  Path\n");
    printf("0x%08X    SB  <superblock>\n", 0);
    
    *end_block = 1;
    return pack_dir(root_dir, end_block) ? NULL : root_dir;
}

static int write_file(saif_block* b, file_to_pack* f, char* name, uint32_t* block, uint32_t* length, uint32_t* checksum)
{
    char* fb = (char*) &b[f->selected_block];
    int fd;
    int err;
    
    strcpy(name, f->name);
    *block = f->selected_block;
    *length = f->length;
    
    if ((fd = open(f->path, O_RDONLY)) == -1)
    {
        err = errno;
        
        printf("Error reading file %s: %s\n", f->path, strerror(err));
        return err;
    }
    
    if (read(fd, fb, f->length) != f->length)
    {
        err = errno == 0 ? EIO : errno;
        
        close(fd);
        
        printf("Error reading file %s: %s\n", f->path, strerror(err));
        return err;
    }
    
    close(fd);
    *checksum = saif_calculate_checksum((uint32_t*) fb, *length);
    
    return 0;
}

static int write_dir(saif_block* b, dir_to_pack* d, char* name, uint32_t* block, uint32_t* length, uint32_t* checksum)
{
    saif_dirent* db = (saif_dirent*) &b[d->selected_block];
    int err;
    
    if (name != NULL)
        strcpy(name, d->name);
    
    *block = d->selected_block;
    *length = (d->files_length + d->dirs_length) * sizeof(saif_dirent);
    
    for (uint32_t i = 0; i < d->files_length; i++)
    {
        saif_dirent* entry = &db[i];
        
        entry->type = SAIF_TYPE_FILE;
        if ((err = write_file(b, d->files[i], entry->name, &entry->block, &entry->length, &entry->checksum)) != 0)
            return err;
    }
    
    for (uint32_t i = 0; i < d->dirs_length; i++)
    {
        saif_dirent* entry = &db[d->files_length + i];
        
        entry->type = SAIF_TYPE_DIR;
        if ((err = write_dir(b, d->dirs[i], entry->name, &entry->block, &entry->length, &entry->checksum)) != 0)
            return err;
    }
    
    *checksum = saif_calculate_checksum((uint32_t*) db, *length);
    
    return 0;
}

static int write_superblock(saif_block* b, dir_to_pack* root_dir)
{
    saif_superblock* sb = (saif_superblock*) &b[0];
    int err;
    
    memcpy(sb->magic, SAIF_MAGIC, sizeof(SAIF_MAGIC));
    sb->revision = SAIF_REVISION;
    
    if ((err = write_dir(b, root_dir, NULL, &sb->root_dir_block, &sb->root_dir_length, &sb->root_dir_checksum)) != 0)
        return err;
    
    sb->header_checksum = saif_calculate_checksum((uint32_t*) sb, sizeof(saif_block));
    
    return 0;
}

static int write_blocks(const char* file, const char* buf, uint32_t s)
{
    int fd;
    int n;
    int nt = 0;
    int err;
    
    if ((fd = open(file, O_WRONLY | O_CREAT, 0644)) == -1)
    {
        err = errno;
        
        printf("Error writing file %s: %s\n", file, strerror(err));
        return err;
    }
    
    while (nt != s)
    {
        if ((n = write(fd, &buf[nt], s - nt)) == -1)
        {
            err = errno;
            
            close(fd);
            
            printf("Error writing file %s: %s\n", file, strerror(err));
            return err;
        }
        
        nt += n;
    }
    
    close(fd);
    return 0;
}

int saif_write(dir_to_pack* root_dir, const char* output_file, uint32_t end_block)
{
    saif_block* b = calloc(end_block, sizeof(saif_block));
    int err;
    
    if ((err = write_superblock(b, root_dir)) != 0)
        return err;
    
    if ((err = write_blocks(output_file, (const char*) b, end_block * sizeof(saif_block))) != 0)
        return err;
    
    return 0;
}
