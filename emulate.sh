qemu-system-i386 -drive file=sa.img,if=floppy,format=raw -m 64 -s -S -cpu Westmere,-de,-syscall,-lm,-vme,enforce -smp 2 -serial stdio
