#include <stdio.h>
#include <string.h>

#include "saif.h"
#include "pack.h"

void print_usage(int argc, char** argv)
{
    printf("Usage: %s <input_dir> <output_file>\n", argv[0]);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        print_usage(argc, argv);
        return 1;
    }
    
    const char* input_dir = argv[1];
    const char* output_file = argv[2];
    uint32_t end_block;
    
    dir_to_pack* d;
    
    if ((d = saif_prepack(input_dir)) == NULL)
        return 1;
    
    if ((d = saif_pack(d, &end_block)) == NULL)
        return 1;
    
    if (saif_write(d, output_file, end_block))
        return 1;
    
    return 0;
}
