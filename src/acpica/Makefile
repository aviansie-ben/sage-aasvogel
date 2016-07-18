INCLUDE_DIR ?= ./include
KERNEL_INCLUDE_DIR ?= ../kernel/include
SRC_DIR ?= .
OBJ_DIR ?= ../../obj/acpica
OUTPUT_ARCHIVE ?= $(OBJ_DIR)/acpica.a

WARNINGS ?=
CFLAGS += -c -ffreestanding -fvisibility=hidden -std=gnu11 -I$(INCLUDE_DIR) -I$(KERNEL_INCLUDE_DIR) -DSAGE_AASVOGEL $(WARNINGS)
CC := i686-elf-gcc

C_FILES := $(shell find $(SRC_DIR) -type f -name "*.c")
HDR_FILES := $(shell find $(INCLUDE_DIR) -type f -name "*.h")
AUX_FILES := Makefile
ALL_FILES := $(C_FILES) $(HDR_FILES) $(AUX_FILES)

OBJ_FILES := $(patsubst %.c,%.c.o,$(C_FILES))
OBJ_FILES := $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(OBJ_FILES))

DEP_FILES := $(patsubst %.o,%.d,$(OBJ_FILES))

.PHONY: all clean dist todolist

all: todolist acpica.a

clean:
	-@$(RM) $(wildcard $(OBJ_FILES) $(DEP_FILES) $(OUTPUT_ARCHIVE) acpica.tgz)

todolist:
	-@for file in $(ALL_FILES:Makefile=); do fgrep -H -n -e TODO $$file; done; true

dist:
	@tar czf acpica.tgz $(patsubst $(SRC_DIR)/%,%,$(ALL_FILES))

acpica.a: $(OBJ_FILES)
	-@$(RM) $(OUTPUT_ARCHIVE)
	@$(AR) rcs $(OUTPUT_ARCHIVE) $(OBJ_FILES)

$(BOOT_DIR)/%: $(SRC_DIR)/% Makefile
	@cp -f $< $@

$(OBJ_DIR)/sa_shim.c.o: $(SRC_DIR)/sa_shim.c Makefile
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -g -D__SOURCE_FILE__=\"$<\" -MF $(patsubst %.o,%.d,$@) -MMD -MP $< -o $@

$(OBJ_DIR)/%.c.o: $(SRC_DIR)/%.c Makefile
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -D__SOURCE_FILE__=\"$<\" -MF $(patsubst %.o,%.d,$@) -MMD -MP $< -o $@

-include $(DEP_FILES)
