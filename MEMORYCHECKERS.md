# Memory Checkers

There are several ways to check your programs for invisble and hard to track error which are caused by memory issues and undefined behaviors.
This page describes several methods how to catch such using [MAME](https://github.com/mamedev/mame) as an example (**Note:** The examples are based on MAME 0.193 and some of them might have been fixed in subsequent versions even not if mentioned).

## Sanitizers

The sanitizers are available via the **-fsanitize** flag in the [GCC](https://gcc.gnu.org/) and [clang](http://clang.llvm.org/) compilers as well as in [XCode](https://developer.apple.com/xcode/) on macOS.
There's also limited supported for these using Windows Subsystem for Linux. These checks are being compiled into the binary and require certain options to be passed to the compiler and linker.

### AddressSanitizer

* [AddressSanitizer (clang)](https://clang.llvm.org/docs/AddressSanitizer.html)
* [AddressSanitizer (Google)](https://github.com/google/sanitizers/wiki/AddressSanitizer)
* [AddressSanitizer (Wikipedia)](https://en.wikipedia.org/wiki/AddressSanitizer)
* [LeakSanitizer (clang)](https://clang.llvm.org/docs/LeakSanitizer.html)
* [SanitizerCommonFlags (Google)](https://github.com/google/sanitizers/wiki/SanitizerCommonFlags)

This sanitizer provides checks to catch memory errors and leaks (**LeakSanitizer** is enabled by default).

For convenience (and to adjust certain settings) MAME has a build flag which passes the sanitozer name to the build system. To enable the **address* sanitizer use
```
make CC=clang SANITIZE=address
```

After this you just need to run the application and the process will exit when it encounters an error and write the infomration to stderr

Example:
```
./mame64 atom -cass 747
```

```
==94035==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x62b000125b27 at pc 0x00000e98d8d7 bp 0x7ffd4111a110 sp 0x7ffd4111a108
READ of size 1 at 0x62b000125b27 thread T0
    #0 0xe98d8d6 in uef_cas_fill_wave(short*, int, unsigned char*) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/uef_cas.cpp:254:22
    #1 0xe8b41b0 in cassette_legacy_construct(cassette_image*, CassetteLegacyWaveFiller const*) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/cassimg.cpp:951:12
    #2 0xe98bbbd in uef_cassette_load(cassette_image*) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/uef_cas.cpp:337:9
    #3 0xe8af997 in cassette_open_choices(void*, io_procs const*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, CassetteFormat const* const*, int, cassette_image**) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/cassimg.cpp:177:8
    #4 0xc6c0a55 in cassette_image_device::internal_load(bool) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp:287:10
    #5 0xc6c0de1 in call_load /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp:258:9
    #6 0xc6c0de1 in non-virtual thunk to cassette_image_device::call_load() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp
    #7 0xe108af6 in device_image_interface::finish_load() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/diimage.cpp:1163:11
    #8 0xe5686c8 in image_manager::postdevice_init() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/image.cpp:233:36
    #9 0xe1f0ca6 in driver_device::device_start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/driver.cpp:207:20
    #10 0xe0e345d in device_t::start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/device.cpp:489:2
    #11 0xe6a1f65 in running_machine::start_all_devices() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:1040:13
    #12 0xe6a005d in running_machine::start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:265:2
    #13 0xe6a2a41 in running_machine::run(bool) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:310:3
    #14 0x8cd10e0 in mame_machine_manager::execute() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/mame.cpp:236:19
    #15 0x8e1e0d3 in cli_frontend::start_execution(mame_machine_manager*, std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > const&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/clifront.cpp:257:22
    #16 0x8e20ee0 in cli_frontend::execute(std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/clifront.cpp:273:3
    #17 0x8cd3717 in emulator_info::start_frontend(emu_options&, osd_interface&, std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/mame.cpp:336:18
    #18 0x8acddf2 in main /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/osd/sdl/sdlmain.cpp:216:9
    #19 0x7fc4a26a182f in __libc_start_main /build/glibc-bfm8X4/glibc-2.23/csu/../csu/libc-start.c:291
    #20 0x1431838 in _start (/mnt/mame/mame64_as+0x1431838)

0x62b000125b27 is located 1 bytes to the right of 26918-byte region [0x62b00011f200,0x62b000125b26)
allocated by thread T0 here:
    #0 0x14d2a23 in malloc /opt/media/clang_nightly/llvm/utils/release/final/llvm.src/projects/compiler-rt/lib/asan/asan_malloc_linux.cc:67:3
    #1 0xe98dbf8 in uef_cas_to_wav_size(unsigned char const*, int) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/uef_cas.cpp:127:23
    #2 0xe8b4079 in cassette_legacy_construct(cassette_image*, CassetteLegacyWaveFiller const*) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/cassimg.cpp:911:18
    #3 0xe98bbbd in uef_cassette_load(cassette_image*) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/uef_cas.cpp:337:9
    #4 0xe8af997 in cassette_open_choices(void*, io_procs const*, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, CassetteFormat const* const*, int, cassette_image**) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/cassimg.cpp:177:8
    #5 0xc6c0a55 in cassette_image_device::internal_load(bool) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp:287:10
    #6 0xc6c0de1 in call_load /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp:258:9
    #7 0xc6c0de1 in non-virtual thunk to cassette_image_device::call_load() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/devices/imagedev/cassette.cpp
    #8 0xe108af6 in device_image_interface::finish_load() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/diimage.cpp:1163:11
    #9 0xe5686c8 in image_manager::postdevice_init() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/image.cpp:233:36
    #10 0xe1f0ca6 in driver_device::device_start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/driver.cpp:207:20
    #11 0xe0e345d in device_t::start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/device.cpp:489:2
    #12 0xe6a1f65 in running_machine::start_all_devices() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:1040:13
    #13 0xe6a005d in running_machine::start() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:265:2
    #14 0xe6a2a41 in running_machine::run(bool) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/emu/machine.cpp:310:3
    #15 0x8cd10e0 in mame_machine_manager::execute() /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/mame.cpp:236:19
    #16 0x8e1e0d3 in cli_frontend::start_execution(mame_machine_manager*, std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > > const&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/clifront.cpp:257:22
    #17 0x8e20ee0 in cli_frontend::execute(std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/clifront.cpp:273:3
    #18 0x8cd3717 in emulator_info::start_frontend(emu_options&, osd_interface&, std::vector<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::allocator<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >&) /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/frontend/mame/mame.cpp:336:18
    #19 0x8acddf2 in main /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/osd/sdl/sdlmain.cpp:216:9
    #20 0x7fc4a26a182f in __libc_start_main /build/glibc-bfm8X4/glibc-2.23/csu/../csu/libc-start.c:291

SUMMARY: AddressSanitizer: heap-buffer-overflow /mnt/mame/build/projects/sdl/mame/gmake-linux-clang/../../../../../src/lib/formats/uef_cas.cpp:254:22 in uef_cas_fill_wave(short*, int, unsigned char*)
Shadow bytes around the buggy address:
  0x0c568001cb10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c568001cb20: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c568001cb30: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c568001cb40: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
  0x0c568001cb50: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
=>0x0c568001cb60: 00 00 00 00[06]fa fa fa fa fa fa fa fa fa fa fa
  0x0c568001cb70: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c568001cb80: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c568001cb90: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c568001cba0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
  0x0c568001cbb0: fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa fa
Shadow byte legend (one shadow byte represents 8 application bytes):
  Addressable:           00
  Partially addressable: 01 02 03 04 05 06 07 
  Heap left redzone:       fa
  Freed heap region:       fd
  Stack left redzone:      f1
  Stack mid redzone:       f2
  Stack right redzone:     f3
  Stack after return:      f5
  Stack use after scope:   f8
  Global redzone:          f9
  Global init order:       f6
  Poisoned by user:        f7
  Container overflow:      fc
  Array cookie:            ac
  Intra object redzone:    bb
  ASan internal:           fe
  Left alloca redzone:     ca
  Right alloca redzone:    cb
```

### UndefinedBehaviorSanitizer

* [UndefinedBehaviorSanitizer (clang)](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
* [SanitizerCommonFlags (Google)](https://github.com/google/sanitizers/wiki/SanitizerCommonFlags)


This sanitizer provides checks to detect undefined behaviors in your application.

For convenience (and to adjust certain settings) MAME has a build flag which passes the sanitozer name to the build system. To enable the **undefined* sanitizer use
```
make CC=clang SANITIZE=undefined
```

Example:
```
./mame64 gba -cart cart 007eon
```

```
../../../../../src/mame/drivers/gba.cpp:761:54: runtime error: index 209 out of bounds for type 'const char *[42]'
```

[Commit](https://github.com/mamedev/mame/commit/762bfd13f9b222bb97d78cb7de7b5dc5ad64628e) which fixes this error.

### ThreadSanitizer

* [ThreadSanitizer (clang)](https://clang.llvm.org/docs/ThreadSanitizer.html)
* [SanitizerCommonFlags (Google)](https://github.com/google/sanitizers/wiki/SanitizerCommonFlags)

**TODO**
 
### MemorySanitizer

This sanitizer provides checks to detect the usage of uninitialized memory. Unfortunately it requires **all** dependent library to be compiled with these checks enabled to work properly.

* [MemorySanitizer (clang)](https://clang.llvm.org/docs/MemorySanitizer.html)

**TODO**

## Dr. Memory

* [Dr. Memory](http://drmemory.org/)
* [Dr. Memory Runtime Option Reference](http://drmemory.org/docs/page_options.html)


Dr. Memory is a multi-platform memory debugger and does not require a special binary to perform these checks. You need a binary with symbols though to get useful output. It will also report memory leaks.

To build MAME with symbols
```
make SYMBOLS=1
```

The application will exit on the first error which is encountered and it will automatically open a results.txt which includes all the information about the error.

Example:
```
drmemory.exe -ignore_kernel -quiet -crash_at_error -crash_at_unaddressable -callstack_max_frames 50 -malloc_max_frames 50 -- mame64 -window atomb -cass 747
```

**Notes:**
* make sure to always run it with **-window** in case the process hangs so you can easily get back to the Desktop
* adding **-light** to the command-line will disable the leak checking.

**TODO: add output**

## valgrind

* [valgrind](http://valgrind.org/)

**TODO**

## Visual Studio Run-Time Checks

* [/RTC (Run-Time Error Checks) - Visual Studio 2017](https://docs.microsoft.com/en-us/cpp/build/reference/rtc-run-time-error-checks)
* [/RTC (Run-Time Error Checks)](https://msdn.microsoft.com/en-us/library/8wtf2dfz.aspx)

A Visual Studio Debug build has some run-time checks compiled in by default which will catch certain memory errors.

**TODO**
