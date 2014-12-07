#!/bin/bash

if [ "$#" -ne 2 ] || [ ! -d "$1" ] || [ ! -d "$2" ]; then
    echo "Usage: $0 <object directory> <output directory>"
    exit 1
fi

source_directory=$(dirname $(readlink -f "$0"))
object_directory=$1
output_directory=$2

asm_build="i686-elf-as -g"
cpp_build="i686-elf-g++ -c -g -O0 -Wall -Werror -ffreestanding -nostdlib -fno-exceptions -fno-rtti -I$source_directory/include"
linker="i686-elf-ld -T $source_directory/linker.ld"

asm_files=$(find $source_directory -name '*.asm')
cpp_files=$(find $source_directory -name '*.cpp')

echo "Building kernel..."

echo "  Compiling assembly files..."
for src in $asm_files
do
    echo "    Compiling ${src/$source_directory\//}..."
    
    # Find the object file name
    obj=${src/$source_directory/$object_directory}
    obj=${obj/.asm/.asm.o}
    
    # Make sure the folder for the object file exists
    if [ ! -d $(dirname $obj) ]
    then
        mkdir -p $(dirname $obj)
    fi
    
    # Call the assembly compiler
    if ! $asm_build $src -o $obj
    then
        exit 1
    fi
done

echo "  Compiling C++ files..."
for src in $cpp_files
do
    echo "    Compiling ${src/$source_directory\//}..."
    
    # Find the object file name
    obj=${src/$source_directory/$object_directory}
    obj=${obj/.cpp/.cpp.o}
    
    # Make sure the folder for the object file exists
    if [ ! -d $(dirname $obj) ]
    then
        mkdir -p $(dirname $obj)
    fi
    
    # Call the C++ compiler
    if ! $cpp_build -D__SOURCE_FILE__='"'${src/$source_directory\//}'"' $src -o $obj
    then
        exit 1
    fi
done

echo "  Running linker..."
if ! $linker -o $output_directory/kernel.bin $(i686-elf-g++ -print-file-name=crtbegin.o) $(find $objdir -name '*.o') $(i686-elf-g++ -print-file-name=crtend.o)
then
    exit 1
fi

echo "  Copying GRUB configuration..."
mkdir $output_directory/boot
cp $source_directory/menu.cfg $output_directory/boot/menu.cfg
