MODULES := kernel
MODULES_CLEAN := $(patsubst %,clean-%,$(MODULES))
MODULES_DOC := $(patsubst %,doc-%,$(MODULES))

IMG_SRC_DIR ?= img
IMG_TEMPLATE ?= template.img

IMG_OUT ?= sa.img

IMG_MNT_DIR ?= /mnt/sa-floppy

QEMU ?= qemu-system-i386
QEMUFLAGS ?= -m 64 -s -cpu Westmere,-de,-syscall,-lm,-vme,enforce -smp 2 -serial file:serial.out \
             -chardev pty,id=gdbs -serial chardev:gdbs
QEMUFLAGS += -drive file=$(IMG_OUT),if=floppy,format=raw

.PHONY: all clean doc $(MODULES) $(MODULES_CLEAN) image emulate

all: $(MODULES)

clean: $(MODULES_CLEAN)

doc: $(MODULES_DOC)

image:
	@cp -f --preserve=ownership $(IMG_TEMPLATE) $(IMG_OUT)
	
	@mkdir $(IMG_MNT_DIR)
	@mount -o loop $(IMG_OUT) $(IMG_MNT_DIR)
	@cp -r $(IMG_SRC_DIR)/* $(IMG_MNT_DIR)
	
	@sleep 1
	@umount $(IMG_MNT_DIR)
	@rm -rf $(IMG_MNT_DIR)

emulate:
	@$(QEMU) $(QEMUFLAGS)

kernel:
	@$(MAKE) -C src/kernel all

clean-kernel:
	@$(MAKE) -C src/kernel clean

doc-kernel:
	@$(MAKE) -C src/kernel doc
