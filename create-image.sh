#!/bin/bash

image_dir="img"
image_template="template.img"
image_out="sa.img"

mount_dir="/mnt/sa-floppy"

# This script must be run as root, since it involves mounting and unmounting
# a loopback floppy device.
if [ "$(id -u)" != "0" ]; then
    if [ -t 0 ] || ! command -v gksudo >/dev/null 2>&1; then
        exec sudo "$0" "$@"
    else
        exec gksudo "$0" "$@"
    fi
fi

echo "Packaging floppy image..."

echo "  Creating new image from template..."
if ! cp --preserve=ownership $image_template $image_out
then
    echo "    ERROR: Failed to copy template!"
    exit 1
fi

echo "  Mounting new image..."
if [ -d $mount_dir ]; then
    echo "    ERROR: Something's already mounted at $mount_dir!"
    exit 1
fi

mkdir $mount_dir
if ! mount -o loop $image_out $mount_dir; then
    rm -rf $mount_dir
    exit 1
fi

echo "  Copying files to new image..."
cp -r $image_dir/* $mount_dir

echo "  Unmounting image..."
sleep 1
umount $mount_dir
rm -rf $mount_dir
