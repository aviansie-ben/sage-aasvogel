#!/bin/bash

if [ "$#" -ne 2 ] || [ ! -d "$1" ] || [ ! -d "$2" ]; then
    echo "Usage: $0 <object directory> <output directory>"
    exit 1
fi

source_directory=$(dirname $(readlink -f "$0"))
object_directory=$1
output_directory=$2

c_warnings="-Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wuninitialized -Wconversion -Wstrict-prototypes -Wno-unused-parameter"
c_build="i686-elf-gcc -c -g -O0 -ffreestanding -std=gnu99 -I$source_directory/include $c_warnings"
linker="i686-elf-ld -T $source_directory/linker.ld -L$(dirname $(i686-elf-gcc -print-file-name=libgcc.a))"
linker_suffix="-lgcc"

asm_files=$(find $source_directory -name '*.s')
c_files=$(find $source_directory -name '*.c')

echo "Building kernel..."

echo "  Compiling assembly files..."
for src in $asm_files
do
    echo "    Compiling ${src/$source_directory\//}..."
    
    # Find the object file name
    obj=${src/$source_directory/$object_directory}
    obj=${obj/.s/.s.o}
    
    # Make sure the folder for the object file exists
    if [ ! -d $(dirname $obj) ]
    then
        mkdir -p $(dirname $obj)
    fi
    
    # Call the assembly compiler
    if ! $c_build -D__SOURCE_FILE__='"'${src/$source_directory\//}'"' $src -o $obj
    then
        exit 1
    fi
done

echo "  Compiling C files..."
for src in $c_files
do
    echo "    Compiling ${src/$source_directory\//}..."
    
    # Find the object file name
    obj=${src/$source_directory/$object_directory}
    obj=${obj/.c/.c.o}
    
    # Make sure the folder for the object file exists
    if [ ! -d $(dirname $obj) ]
    then
        mkdir -p $(dirname $obj)
    fi
    
    # Call the C compiler
    if ! $c_build -D__SOURCE_FILE__='"'${src/$source_directory\//}'"' $src -o $obj
    then
        exit 1
    fi
done

echo "  Running linker..."
if ! $linker -o $output_directory/kernel.bin $(find $objdir -name '*.o') $linker_suffix
then
    exit 1
fi

echo "  Copying GRUB configuration..."
mkdir $output_directory/boot
cp $source_directory/menu.cfg $output_directory/boot/menu.cfg
