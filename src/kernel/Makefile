INCLUDE_DIR ?= ./include
SRC_DIR ?= .
OBJ_DIR ?= ../../obj/kernel
KERNEL_BIN ?= ../../img/kernel.bin
BOOT_DIR ?= ../../img/boot
DOC_DIR ?= ../../doc/kernel

VERSION_SUFFIX ?= -git-$(shell git describe --always)

WARNINGS ?= -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
            -Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
            -Wuninitialized -Wconversion -Wstrict-prototypes -Wno-unused-parameter
CFLAGS += -c -g -ffreestanding -std=gnu11 -I$(INCLUDE_DIR) -DVERSION_SUFFIX=\"$(VERSION_SUFFIX)\" $(WARNINGS)
CC := i686-elf-gcc

LDFLAGS += -T $(SRC_DIR)/linker.ld -L$(dir $(shell i686-elf-gcc -print-file-name=libgcc.a))
LD := i686-elf-ld

DOXYGEN := doxygen

BOOT_FILES := $(SRC_DIR)/menu.cfg
C_FILES := $(shell find $(SRC_DIR) -type f -name "*.c")
ASM_FILES := $(shell find $(SRC_DIR) -type f -name "*.S")
HDR_FILES := $(shell find $(INCLUDE_DIR) -type f -name "*.h")
AUX_FILES := Makefile linker.ld
ALL_FILES := $(BOOT_FILES) $(C_FILES) $(ASM_FILES) $(HDR_FILES) $(AUX_FILES)

BOOT_OUT_FILES := $(patsubst $(SRC_DIR)/%,$(BOOT_DIR)/%,$(BOOT_FILES))
OBJ_FILES := $(patsubst %.c,%.c.o,$(C_FILES)) $(patsubst %.S,%.S.o,$(ASM_FILES))
OBJ_FILES := $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(OBJ_FILES))

DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

.PHONY: all clean dist todolist doc

all: todolist kernel.bin boot-files

clean:
	-@$(RM) $(wildcard $(OBJ_FILES) $(DEP_FILES) $(BOOT_OUT_FILES) $(KERNEL_BIN) doxygen-generated.cfg kernel.tgz)
	-@$(RM) -rf $(DOC_DIR)

todolist:
	-@for file in $(ALL_FILES:Makefile=); do fgrep -H -e TODO $$file; done; true

dist:
	@tar czf kernel.tgz $(patsubst $(SRC_DIR)/%,%,$(ALL_FILES))

doc: doxygen-generated.cfg $(C_FILES) $(HDR_FILES)
	@doxygen doxygen-generated.cfg

boot-files: $(BOOT_OUT_FILES)

kernel.bin: $(OBJ_FILES)
	@$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJ_FILES) -lgcc

doxygen-generated.cfg: doxygen.cfg Makefile
	@cp -f doxygen.cfg doxygen-generated.cfg
	@echo "OUTPUT_DIRECTORY = $(DOC_DIR)" >> doxygen-generated.cfg
	@echo "STRIP_FROM_PATH = $(SRC_DIR) $(INCLUDE_DIR)" >> doxygen-generated.cfg
	@echo "STRIP_FROM_INC_PATH = $(INCLUDE_DIR)" >> doxygen-generated.cfg
	@echo "INPUT = $(SRC_DIR)" >> doxygen-generated.cfg

$(BOOT_DIR)/%: $(SRC_DIR)/% Makefile
	@cp -f $< $@

$(OBJ_DIR)/%.c.o: $(SRC_DIR)/%.c Makefile
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -D__SOURCE_FILE__=\"$<\" -MF $(patsubst %.o,%.d,$@) -MMD -MP $< -o $@

$(OBJ_DIR)/%.S.o: $(SRC_DIR)/%.S Makefile
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -D__SOURCE_FILE__=\"$<\" -D__ASSEMBLY__ -MF $(patsubst %.o,%.d,$@) -MMD -MP $< -o $@

-include $(DEP_FILES)
