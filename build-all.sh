#!/bin/bash

image_dir="img"
object_dir="obj"
source_dir="src"

# Clean out directories
rm -rf $image_dir/*
rm -rf $object_dir/*

# Build the kernel
mkdir $object_dir/kernel
if ! $source_dir/kernel/build.sh $object_dir/kernel $image_dir; then
    exit 1
fi

# Create the image
./create-image.sh
