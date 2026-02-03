# Cytrus IR (Libretro Core)
Cytrus is a Nintendo 3DS emulation core, optimized for systems without JIT (such as iOS) using a Static IR interpreter and a multi-threaded Software Renderer.

## RetroArch Integration

To use this core in RetroArch:
1. Compile the core using `make -f Makefile.libretro PLATFORM=ios` (for iOS) or `make -f Makefile.libretro` (for Linux).
2. Copy the resulting library (e.g., `cytrus_libretro_ios.dylib`) to your RetroArch `modules` or `cores` directory.
3. Copy `cytrus_libretro.info` to your RetroArch `info` directory.
4. Restart RetroArch, and the core will appear as "Nintendo - 3DS (Cytrus IR)".

## Credits
Cytrus is built on top of the **Citra** emulator formally developed by the **Citra Team** and later picked up by the **Azahar Team**.
