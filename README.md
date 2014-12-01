# Project Sage Aasvogel

## What is it?

Project Sage Aasvogel is a basic operating system written using Assembly language and C++. This was mainly written for educational purposes, so don't expect it to be of much practical use.

## Disclaimer

This is an experimental OS designed solely for educational purposes. I am not in **any way responsible** for anything that happens to any hardware you decide to run this code on! This code is **not thoroughly tested** and will most likely cause your processor to melt and your disk drive to explode.

## Building Sage Aasvogel

You can build the entire Sage Aasvogel project by running the `build-all.sh` script. This will build all components of the system and then run the `create-image.sh` to package a new floppy image for testing. By default, the image is packaged in the project directory as `sa.img`, but this can be changed in the script.

Note that the creation of the floppy disk image using `create-image.sh` requires root permissions in order to be able to mount the disk image as a loopback device. If you run `create-image.sh` as a non-root user, it will automatically attempt to run itself as root using `sudo` or `gksudo` if the script is run without a TTY, e.g. when run by an IDE.

In order to be able to build Sage Aasvogel, you'll need to have `binutils` for the GNU linker, `g++` to compile the C++ source files, and `nasm` to compile the assembly source files.

## Testing Sage Aasvogel

The preferred method for testing Sage Aasvogel is by using a system emulator such as [qemu](http://www.qemu.org/) with an attached debugger. The included `emulate.sh` script will start qemu with 64MiB of RAM and the floppy disk image `sa.img` mounted on the first floppy drive. Once started, qemu will listen on port `1234` for a debugger to connect.