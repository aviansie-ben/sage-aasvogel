CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
INCLUDE(ExternalProject)

# This project is set to use C and ASM despite not directly using those languages. This is necessary
# since this project file will be playing around with CMAKE_C_FLAGS and CMAKE_ASM_FLAGS; not
# declaring the languages here leads to those variables being overwritten the first time the
# compiler check is run and causing weird intermittent issues.
PROJECT("Sage Aasvogel" C ASM)

# Version options
OPTION(VERSION_INCLUDE_GIT_INFO "Include the git hash in the version number" ON)

# klog options
OPTION(KLOG_OUTPUT_TO_HOST_FILE "Output kernel logs to serial.out" ON)

# Extra debug output
OPTION(MMAP_DEBUG "Print physical memory map on startup" OFF)
OPTION(SCHED_DEBUG "Store debug info about threads and processes" OFF)
OPTION(KMEM_EARLY_DEBUG "Track usage of kmalloc_early" OFF)

# Kernel GDB stub
OPTION(KERNEL_GDB_STUB "Include a GDB stub with the kernel" OFF)
OPTION(KERNEL_GDB_WAIT "On boot, immediately break and wait for GDB" OFF)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

SET(SAGE_AASVOGEL_IMG_DIRECTORY ${CMAKE_BINARY_DIR}/img)
SET(SAGE_AASVOGEL_DOC_DIRECTORY ${CMAKE_BINARY_DIR}/doc)
SET(SAGE_AASVOGEL_INITRD_DIRECTORY ${CMAKE_BINARY_DIR}/initrd)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${SAGE_AASVOGEL_IMG_DIRECTORY})
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES ${SAGE_AASVOGEL_INITRD_DIRECTORY})

SET(KERNEL_INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/kernel/include)

SET(WARNINGS "-Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings \
              -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls -Wnested-externs \
              -Winline -Wno-long-long -Wuninitialized -Wconversion -Wstrict-prototypes \
              -Wno-unused-parameter -Wno-maybe-uninitialized")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -ffreestanding -std=gnu11 -D__SOURCE_FILE__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"' ${WARNINGS}")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g -ffreestanding -D__ASSEMBLY__ -D__SOURCE_FILE__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

ADD_DEFINITIONS(-DSAGE_AASVOGEL)

EXECUTE_PROCESS(COMMAND i686-elf-gcc "-print-file-name=libgcc.a" OUTPUT_VARIABLE LIBGCC_LOCATION)
STRING(STRIP ${LIBGCC_LOCATION} LIBGCC_LOCATION)
SET(CMAKE_C_STANDARD_LIBRARIES ${LIBGCC_LOCATION})

SET(QEMU_FLAGS -m 64 -s -cpu Westmere,-de,-syscall,-lm,-vme,enforce -smp 2 -drive file=sa.img,if=floppy,format=raw)

SET(NEXT_SERIAL_PORT 0)

IF(VERSION_INCLUDE_GIT_INFO)
    EXEC_PROGRAM(
        "git"
        ${CMAKE_CURRENT_SOURCE_DIR}
        ARGS rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_VERSION
    )

    ADD_DEFINITIONS(-DVERSION_SUFFIX="-git-${GIT_VERSION}")
ENDIF()

IF(KLOG_OUTPUT_TO_HOST_FILE)
    SET(QEMU_FLAGS ${QEMU_FLAGS} -serial file:serial.out)

    SET(KLOG_SERIAL_PORT ${NEXT_SERIAL_PORT})
    MATH(EXPR NEXT_SERIAL_PORT ${NEXT_SERIAL_PORT}+1)
ENDIF()

IF(KERNEL_GDB_STUB)
    SET(QEMU_FLAGS ${QEMU_FLAGS} -chardev pty,id=gdbs -serial chardev:gdbs)

    SET(KERNEL_GDB_STUB_SERIAL_PORT ${NEXT_SERIAL_PORT})
    MATH(EXPR NEXT_SERIAL_PORT ${NEXT_SERIAL_PORT}+1)
ENDIF()

ADD_SUBDIRECTORY(acpica)
ADD_SUBDIRECTORY(kernel)

# saif_packer needs to be run as part of the build process, meaning that it must be compiled to run
# on the host machine rather than the target machine. In order to do this, it must use the system's
# default toolchain rather than the cross-compiler toolchain used by everything else here, which can
# only be accomplished by including it as an external project.
EXTERNALPROJECT_ADD(
    saif_packer
    DOWNLOAD_COMMAND ""
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/saif_packer
    BINARY_DIR ${CMAKE_BINARY_DIR}/saif_packer
    INSTALL_COMMAND ""
    STEP_TARGETS build
    BUILD_ALWAYS 1
    EXCLUDE_FROM_ALL 1
)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_BINARY_DIR}/saif_packer/saif_packer DEPENDS saif_packer)

SET(IMG_TEMPLATE ${CMAKE_SOURCE_DIR}/../template.img)
SET(IMG_MNT_DIR /mnt/sa-floppy)

ADD_CUSTOM_TARGET(doc
    DEPENDS kernel-doc
)

ADD_CUSTOM_COMMAND(OUTPUT ${SAGE_AASVOGEL_INITRD_DIRECTORY}/test.txt
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SAGE_AASVOGEL_INITRD_DIRECTORY}
    COMMAND echo "This is a test file" > ${SAGE_AASVOGEL_INITRD_DIRECTORY}/test.txt
)

ADD_CUSTOM_COMMAND(OUTPUT ${SAGE_AASVOGEL_IMG_DIRECTORY}/boot/initrd
    COMMAND ${CMAKE_BINARY_DIR}/saif_packer/saif_packer ${SAGE_AASVOGEL_INITRD_DIRECTORY} ${SAGE_AASVOGEL_IMG_DIRECTORY}/boot/initrd
    DEPENDS ${CMAKE_BINARY_DIR}/saif_packer/saif_packer
            ${SAGE_AASVOGEL_INITRD_DIRECTORY}/test.txt
)

ADD_CUSTOM_TARGET(image ALL
    DEPENDS kernel-image
            ${SAGE_AASVOGEL_IMG_DIRECTORY}/boot/initrd
)

ADD_CUSTOM_TARGET(pack-image
    COMMAND cp -f --preserve=ownership ${IMG_TEMPLATE} sa.img
    COMMAND mkdir ${IMG_MNT_DIR}
    COMMAND mount -o loop sa.img ${IMG_MNT_DIR}
    COMMAND cp -r ${SAGE_AASVOGEL_IMG_DIRECTORY}/* ${IMG_MNT_DIR}
    COMMAND sleep 1
    COMMAND umount ${IMG_MNT_DIR}
    COMMAND rm -rf ${IMG_MNT_DIR}
    DEPENDS image
)

ADD_CUSTOM_TARGET(emulate
    COMMAND ${QEMU_COMMAND} ${QEMU_FLAGS}
)
