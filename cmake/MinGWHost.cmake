# CMake toolchain for building the ReXGlue HOST tools (the codegen recompiler)
# as native Windows x86_64 binaries, using the mingw-w64 cross-clang that ships
# with devkitPro's MSYS2.
#
# Why this exists: the recompiler must RUN on the host to produce the
# architecture-independent generated/ C++ before the Switch target build can use
# it. There is no MSVC toolchain on this machine, and building under the plain
# MSYS2/Cygwin runtime fails because SDL3 classifies Cygwin as Windows while
# Cygwin lacks the MSVC CRT (_beginthreadex). Targeting mingw-w64 instead gives
# a genuine _WIN32 build, so SDL3/FFmpeg/rexglue all take their supported
# Windows code paths.
#
# Usage (run inside devkitPro's MSYS2 shell):
#   cmake -S . -B out/build/host-mingw \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/MinGWHost.cmake -DCMAKE_BUILD_TYPE=Release \
#         -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON
#   PATH=/opt/x86_64-w64-mingw32/bin:$PATH \
#     cmake --build out/build/host-mingw --target generate-all
#
# NOTE: the toolchain's Clang links against libc++, so the produced rexglue.exe
# needs libc++.dll / libunwind.dll from ${MINGW_HOST_ROOT}/bin at runtime. Keep
# that directory on PATH when building the codegen targets (which execute the
# tool), or add -static to the link flags for a self-contained binary.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED MINGW_HOST_ROOT)
    set(MINGW_HOST_ROOT "/opt/x86_64-w64-mingw32" CACHE PATH "mingw-w64 cross toolchain root")
endif()

set(_MINGW_BIN "${MINGW_HOST_ROOT}/bin")
set(_MINGW_PREFIX "x86_64-w64-mingw32-")

set(CMAKE_C_COMPILER   "${_MINGW_BIN}/${_MINGW_PREFIX}clang")
set(CMAKE_CXX_COMPILER "${_MINGW_BIN}/${_MINGW_PREFIX}clang++")
set(CMAKE_RC_COMPILER  "${_MINGW_BIN}/${_MINGW_PREFIX}windres")
set(CMAKE_AR           "${_MINGW_BIN}/${_MINGW_PREFIX}ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB       "${_MINGW_BIN}/${_MINGW_PREFIX}ranlib"  CACHE FILEPATH "")
set(CMAKE_NM           "${_MINGW_BIN}/${_MINGW_PREFIX}nm"      CACHE FILEPATH "")
set(CMAKE_OBJCOPY      "${_MINGW_BIN}/${_MINGW_PREFIX}objcopy" CACHE FILEPATH "")
set(CMAKE_STRIP        "${_MINGW_BIN}/${_MINGW_PREFIX}strip"   CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH "${MINGW_HOST_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The produced .exe files are native Windows binaries, so the build machine can
# execute them directly (needed for the codegen custom commands).
set(CMAKE_CROSSCOMPILING_EMULATOR "")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
