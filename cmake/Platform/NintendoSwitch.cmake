# cmake/Platform/NintendoSwitch.cmake
# CMake Platform definition for Nintendo Switch (Horizon OS / libnx / devkitA64)
#
# This file silences the "System is unknown to cmake" warnings and provides
# enough platform information for third-party dependencies (SDL3, etc.) to
# configure themselves correctly for a bare-metal-ish AArch64 target.

set(NINTENDO_SWITCH TRUE)
set(SWITCH TRUE)

# Horizon OS (newlib + libnx) is POSIX-like. Mark the platform as UNIX so that
# the many `elseif(UNIX)` branches across the tree select their POSIX sources:
# without this, rexglue's entire posix platform layer (fiber_posix, socket_posix,
# threading_posix, ...) and glslang's OSDependent target are silently dropped,
# producing hollow archives that fail to link. rexglue already anticipates this
# (its UNIX branch guards desktop-only libs behind `if(NOT SWITCH)`).
set(UNIX 1)
set(CMAKE_HOST_UNIX 1)

# Executables on Switch are ELF files (elf2nro converts them to .nro)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

# No shared libraries — Switch homebrew links everything statically
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Switch builds are always static" FORCE)

# -------------------------------------------------------------------
# Thread support
# devkitA64 ships a newlib + ctrulib-style pthread shim via libnx.
# Pre-populate the variables that FindThreads.cmake would normally
# detect via try_compile (which doesn't work for cross-compilation).
# -------------------------------------------------------------------
set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "Thread libs (built into libnx)")
set(CMAKE_HAVE_THREADS_LIBRARY TRUE CACHE BOOL "libnx provides thread support")
set(CMAKE_USE_PTHREADS_API    TRUE CACHE BOOL "libnx exposes the pthreads API")
set(CMAKE_USE_WIN32_THREADS_API FALSE CACHE BOOL "Not Windows")
set(Threads_FOUND TRUE)

# -------------------------------------------------------------------
# System capability hints
# These prevent SDL3 and other libs from running try_compile probes
# that always fail when cross-compiling to an unknown system.
# -------------------------------------------------------------------
set(HAVE_LIBC TRUE)
set(HAVE_POSIX_SPAWN FALSE)   # Horizon OS has no posix_spawn
set(HAVE_VFORK        FALSE)  # Horizon OS has no vfork

# Dynamic linker — not applicable (static only)
set(CMAKE_DL_LIBS "")

# Shared lib conventions (needed internally by CMake even for static builds)
set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
