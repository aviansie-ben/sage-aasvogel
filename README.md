# Project Sage Aasvogel

## What is it?

Project Sage Aasvogel is a basic operating system written using x86 Assembly language and C. This was mainly written for educational purposes, so don't expect it to be of much practical use.

## Disclaimer

This is an experimental OS designed solely for educational purposes. I am not in **any way responsible** for anything that happens to any hardware you decide to run this code on! This code is **not thoroughly tested** and will most likely cause your processor to melt and your disk drive to explode.

## Building Sage Aasvogel

Firstly, it is important to note that the Sage Aasvogel build process currently only works on distibutions of Linux. While the CMake build files could probably be made to work on other systems (e.g. Windows), I haven't found it worth the effort to consider porting them.

In order to build Sage Aasvogel, you will first need to install a number of development tools. Firstly, you will need CMake (3.0 or higher) in order to create the necessary Makefiles. You will also need a cross compilation toolchain for the `i686-elf` platform; specifically, you need GNU Binutils and GCC. If you don't know how to build a cross compiler, the OSDev Wiki has a good [tutorial](http://wiki.osdev.org/GCC_Cross-Compiler) on how to build one. Once your cross compiler is built, you must modify your `PATH` environment variable to ensure that `i686-elf-gcc` and `i686-elf-ld` are on the path.

The tested development environment for Sage Aasvogel uses GCC 6.1.0 and GNU Binutils 2.26 as its cross compiler. While everything _should_ work with slightly older (or newer, in the future) versions, there's no guarantees that everything will work properly.

Now that your system environment is ready to run the build, you should run CMake to create the necessary Makefiles to build the system. Make sure that you do this in a `build` subdirectory to avoid cluttering your working directory with build artifacts. In order to be able to cross-compile properly, you need to tell CMake that it should be using a custom toolchain for compilation. This is done by running CMake with the following command (assuming you're in the `build` subdirectory):

```
cmake ../src -DCMAKE_TOOLCHAIN_FILE=../src/toolchain.cmake [OTHER OPTIONS]
```

This tells CMake to use Sage Aasvogel's default `toolchain.cmake` file to determine information about the toolchain. Of course, this assumes that you're using the default cross compilation toolchain. If for some reason, you need to make modifications to toolchain options, you should create a copy of the `toolchain.cmake` file in your `build` directory, modify it accordingly, and use it in place of the default toolchain file.

Several options can be passed to CMake to control the compilation of Sage Aasvogel (e.g. to enable more verbose debug messages for certain modules). A basic description of the available options can be found at the top of the `src/CMakeLists.txt` file. To turn on one of these options, you need to pass a variable definition to CMake. For instance, if you wanted to enable the kernel GDB stub, you would add the following to the end of your cmake command:

```
-DKERNEL_GDB_STUB=ON
```

Now that you've created the necessary Makefiles, you should be ready to build Sage Aasvogel. Simply running `make` in your `build` directory will populate the `build/img` directory with the files that need to go on the bootable disk image.

Once the `img` directory is ready, it needs to be moved onto a disk image. This can be done by running `make pack-image`. Note that this command needs to be run with root privileges to be able to mount a loopback floppy drive. There's probably a way to do this without requiring root, but I haven't found one thus far.

Now you should have a floppy disk image at `build/sa.img` with Sage Aasvogel installed on it. If you're old-school, you could put this onto an actual floppy and boot it on a real machine, but it's much more likely that you'll want to run an emulator.

## Testing Sage Aasvogel

The preferred method for testing Sage Aasvogel is to use an emulator such as [QEMU](http://www.qemu.org/). In fact, the Makefiles created when you built Sage Aasvogel have a `make emulate` command you can run to start QEMU with the default options required to run Sage Aasvogel. Note that the default toolchain file assumes that your version of QEMU can be run usng the `qemu-system-i386` command; if this is not the correct command, you'll need to either alias that command to your command for starting QEMU or use a custom toolchain file with the correct command to run QEMU.

The default options used by `make emulate` to run QEMU will cause QEMU to listen for GDB connections on port `1234`. This allows for low-level debugging and should always work, even in the event of a full system lockup. However, using this stub to debug has some drawbacks; specifically, if an unhandled exception occurs, the debugger will debug the fault handler (which simply displays an error message) rather than the faulting code. As a result, this method of debugging should be used only when debugging through other methods is not feasible.

The preferred option for GDB debugging at this point is to use Sage Aasvogel's built-in GDB stub. This can be enabled by setting the `KERNEL_GDB_STUB` option to `ON` through CMake. When enabled, QEMU will allocate a PTY to use for communicating with the GDB stub and will print the path of the PTY to stdout upon startup. You can then use GDB remote debugging to connect to this PTY for debugging.

Note that interrupting the system by connecting GDB during early phases of startup is not possible unless `KERNEL_GDB_WAIT` is also set to `ON`. This will cause the GDB stub to immediately stop execution when it first initializes, allowing debugging before serial port interrupts are ready.

Note that Sage Aasvogel's built-in GDB stub may not work under certain conditions. For instance, interrupting a deadlock when interrupts are disabled is not generally possible. If the GDB stub stops responding, you can disconnect from it and connect to QEMU's GDB stub to definitively interrupt execution and find where the code is hanging.

Also, take note that your system's version of GDB may not work correctly to debug Sage Aasvogel; this is especially a problem for 64-bit machines, where the system's GDB will generally target `x86_64` instead of `i686`; while this may work correctly, you may run into strange issues with debugging. For this reason, it is strongly recommended that you build a version of GDB from source whose debugging target is the `i686-elf` platform and use that version of GDB.

## Building Documentation

Some parts of the Sage Aasvogel kernel have associated Doxygen documentation comments in their header files. While it is possible to view this documentation directly in the relevant header files, it is usually more pleasant to build HTML documentation. This can be done by executing `make doc` in your `build` directory. After this, the relevant documentation will be available in the `build/docs` directory.

Note that in order to build the documentation, you will need to have Doxygen installed. Thankfully, no special version of Doxygen is needed and you can simply use the version available from your distro's package repository.

## License

Sage Aasvogel is released as open-source software under the terms of the GNU General Public License version 3. You can find a full copy of the license in markdown format in `LICENSE.md` or the official version on the [GNU Website](https://www.gnu.org/licenses/gpl.html).
