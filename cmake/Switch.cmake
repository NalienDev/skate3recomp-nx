# CMake Toolchain File for Nintendo Switch (devkitA64 / libnx)
# Configured for devkitPro on D:\devkitpro

# Resolve DEVKITPRO — prefer environment variable (MSYS2 sets it as POSIX path e.g. /d/devkitpro)
if(DEFINED ENV{DEVKITPRO})
    set(DEVKITPRO "$ENV{DEVKITPRO}")
elseif(EXISTS "/d/devkitpro")
    set(DEVKITPRO "/d/devkitpro")
elseif(EXISTS "/d/devkitPro")
    set(DEVKITPRO "/d/devkitPro")
elseif(EXISTS "D:/devkitpro")
    set(DEVKITPRO "D:/devkitpro")
elseif(EXISTS "D:/devkitPro")
    set(DEVKITPRO "D:/devkitPro")
elseif(EXISTS "C:/devkitpro")
    set(DEVKITPRO "C:/devkitpro")
else()
    message(FATAL_ERROR
        "Cannot find devkitPro! Set the DEVKITPRO environment variable before running CMake.\n"
        "In MSYS2: export DEVKITPRO=/d/devkitpro")
endif()

# Normalize to CMake path (forward slashes, no trailing slash)
file(TO_CMAKE_PATH "${DEVKITPRO}" DEVKITPRO)
string(REGEX REPLACE "/+$" "" DEVKITPRO "${DEVKITPRO}")
message(STATUS "Using DEVKITPRO: ${DEVKITPRO}")


# Tell CMake where to find Platform/NintendoSwitch.cmake
# CMAKE_CURRENT_LIST_DIR is d:/Projetos/skate3recomp-nx/cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

set(CMAKE_SYSTEM_NAME NintendoSwitch)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Disable precompiled headers globally since MSYS2 paths cause native toolchain compile failures
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

# Disable OS APIs in fmt for Nintendo Switch (lacks POSIX file descriptors)
set(FMT_OS OFF CACHE BOOL "Disable OS APIs in fmt for Switch" FORCE)

# Force fmt to use standard C fallback file wrapper instead of Apple/BSD-specific direct buffer writes
add_compile_definitions(FMT_USE_FALLBACK_FILE=1)

# Disable exception handling in spdlog (Switch toolchain uses -fno-exceptions)
set(SPDLOG_NO_EXCEPTIONS ON CACHE BOOL "Disable spdlog exceptions for Switch" FORCE)
add_compile_definitions(SPDLOG_NO_EXCEPTIONS)

# Provide Switch-specific stubs for POSIX headers missing in newlib
# (e.g. dlfcn.h required by volk; not present in devkitA64's sysroot)
include_directories(BEFORE SYSTEM "${CMAKE_CURRENT_LIST_DIR}/switch_stubs")

# -ftls-model=local-exec is REQUIRED, not an optimization: devkitA64's linker
# mis-relaxes the default initial-exec TLS access sequence into `ldr x0,[x0,x0]`
# (an effective load from 2*thread_pointer), which faults on the first access to
# any `thread_local`. It bit spdlog's os::thread_id() here. A static PIE has a
# single TLS template shared by every thread, so local-exec is always valid.
# Must be in ARCH so it reaches every TU, including thirdparty archives
# (libspdlog.a) that have their own thread_locals.
set(ARCH "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -ftls-model=local-exec")
# NOTE: exceptions and RTTI are LEFT ENABLED. The rexglue runtime and several
# bundled libraries (Vulkan-Hpp, tomlplusplus, cli11, utfcpp, glslang) rely on
# them, and the reference Switch ports (Marathon/Unleashed-NX) build with them
# on. -Wno-psabi silences noisy AArch64 SIMD ABI warnings from devkitA64.
# _GNU_SOURCE raises newlib's feature-visibility level so POSIX/GNU libc
# functions the runtime uses (timegm, setenv, strnlen, fileno, ...) are declared.
set(CMAKE_C_FLAGS_INIT "${ARCH} -O2 -ffunction-sections -fdata-sections -D__SWITCH__ -D_GNU_SOURCE -Wno-psabi")
set(CMAKE_CXX_FLAGS_INIT "${ARCH} -O2 -ffunction-sections -fdata-sections -D__SWITCH__ -D_GNU_SOURCE -Wno-psabi")

set(DEVKITA64 "${DEVKITPRO}/devkitA64")
set(LIBNX "${DEVKITPRO}/libnx")
set(PORTLIBS "${DEVKITPRO}/portlibs/switch")

# Link against libnx via its spec file. switch.specs supplies the Horizon linker
# script, crt0 startup and the newlib syscall backend. Note it does NOT inject
# -lnx itself, so libnx must be linked explicitly (below). --allow-multiple-
# definition tolerates duplicate symbols from our libc shims (mmap/ucontext)
# overlapping weak newlib stubs.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-specs=${LIBNX}/switch.specs -L${LIBNX}/lib -L${PORTLIBS}/lib -Wl,--gc-sections -Wl,--allow-multiple-definition")

# libnx provides __aarch64_read_tp (the soft-TLS thread-pointer helper every
# -mtp=soft TLS access calls) plus the Horizon WSI backend used by the NVK ICD
# (nwindow*/framebuffer*/nvFence*/armDCacheFlush). Static archives such as
# libspdlog.a and libvulkan.a reference these, so -lnx must appear AFTER them on
# the link line. CMAKE_*_STANDARD_LIBRARIES is emitted last, guaranteeing that.
string(APPEND CMAKE_C_STANDARD_LIBRARIES " -lnx")
string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -lnx")
# NOTE: -lnx is now appended to EVERY link, including shared libraries, whose
# link flags lack the libnx -L search path (it lives in the exe flags above).
# The only shared library in this build is Mesa's SPIRV-Tools-shared.so, which
# nothing links and Horizon cannot load; the root CMakeLists excludes it from
# the build on Switch rather than teaching every shared link to find libnx.

if(CMAKE_HOST_WIN32)
    set(EXE_SUFFIX ".exe")
else()
    set(EXE_SUFFIX "")
endif()

set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc${EXE_SUFFIX}")
set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++${EXE_SUFFIX}")
set(CMAKE_AR "${DEVKITA64}/bin/aarch64-none-elf-ar${EXE_SUFFIX}" CACHE FILEPATH "Archiver")
set(CMAKE_AS "${DEVKITA64}/bin/aarch64-none-elf-as${EXE_SUFFIX}" CACHE FILEPATH "Assembler")
set(CMAKE_RANLIB "${DEVKITA64}/bin/aarch64-none-elf-ranlib${EXE_SUFFIX}" CACHE FILEPATH "Ranlib")
set(CMAKE_OBJCOPY "${DEVKITA64}/bin/aarch64-none-elf-objcopy${EXE_SUFFIX}" CACHE FILEPATH "Objcopy")

set(CMAKE_FIND_ROOT_PATH "${PORTLIBS}" "${LIBNX}" "${DEVKITA64}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include_directories(SYSTEM
    "${LIBNX}/include"
    "${PORTLIBS}/include"
)

link_directories(
    "${LIBNX}/lib"
    "${PORTLIBS}/lib"
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# -------------------------------------------------------------------
# Pre-configure SDL3 for Nintendo Switch to avoid failing try_compile
# probes on features that Horizon OS doesn't support.
# -------------------------------------------------------------------
set(SDL_SHARED            OFF CACHE BOOL "" FORCE)
set(SDL_STATIC            ON  CACHE BOOL "" FORCE)
set(SDL_TESTS             OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL           OFF CACHE BOOL "" FORCE)
set(SDL_LIBC              ON  CACHE BOOL "" FORCE)
set(SDL_PTHREADS          ON  CACHE BOOL "" FORCE)
set(SDL_PTHREADS_SEM      ON  CACHE BOOL "" FORCE)
set(SDL_POSIX_SPAWN       OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK          OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC            OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR            OFF CACHE BOOL "" FORCE)
set(SDL_POWER             OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO             OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO             ON  CACHE BOOL "" FORCE)
set(SDL_VULKAN            ON  CACHE BOOL "" FORCE)
set(SDL_OPENGL            OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES          OFF CACHE BOOL "" FORCE)
set(SDL_WAYLAND           OFF CACHE BOOL "" FORCE)
set(SDL_X11               OFF CACHE BOOL "" FORCE)
set(SDL_COCOA             OFF CACHE BOOL "" FORCE)
set(SDL_DIRECTX           OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_D3D        OFF CACHE BOOL "" FORCE)
set(SDL_RENDER_METAL      OFF CACHE BOOL "" FORCE)
set(HAVE_POSIX_SPAWN      FALSE CACHE BOOL "" FORCE)
set(HAVE_VFORK            FALSE CACHE BOOL "" FORCE)
