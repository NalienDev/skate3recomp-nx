// sys/mman.h - Nintendo Switch / devkitA64 (newlib) shim
//
// Horizon OS has no POSIX mmap. This provides the constants and prototypes the
// rexglue guest-memory layer (memory_posix.cpp, mapped_memory_posix.cpp) needs,
// backed by the allocation-based implementation in src/switch/mman_aarch64.cpp.
//
// IMPORTANT: this is a COMPILE-enabling shim. Anonymous and file-backed
// mappings are honored via page-aligned allocation, and mprotect/madvise/msync
// are no-ops. MAP_FIXED requests to a specific host address are NOT guaranteed,
// so the guest-memory system needs a proper libnx virtmem/svcMapMemory backend
// before the recompiled game can actually run. See [[switch-guest-memory]].
#pragma once

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANON      0x20
#define MAP_ANONYMOUS 0x20
// Deliberately do NOT define MAP_FIXED_NOREPLACE: callers guard on
// `#if defined(MAP_FIXED_NOREPLACE)` and fall back to plain MAP_FIXED.

#define MAP_FAILED ((void *)-1)

#define MADV_NORMAL     0
#define MADV_RANDOM     1
#define MADV_SEQUENTIAL 2
#define MADV_WILLNEED   3
#define MADV_DONTNEED   4
#define MADV_FREE       8

#define MS_ASYNC      0x1
#define MS_INVALIDATE 0x2
#define MS_SYNC       0x4

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);
int   mprotect(void *addr, size_t length, int prot);
int   madvise(void *addr, size_t length, int advice);
int   msync(void *addr, size_t length, int flags);

// POSIX shared-memory objects. Horizon has no /dev/shm; these are stubs (see
// mman_aarch64.cpp) and shm_open always fails, so callers fall back to their
// invalid-handle path. Part of the guest-memory work in [[switch-guest-memory]].
int   shm_open(const char *name, int oflag, mode_t mode);
int   shm_unlink(const char *name);

#ifdef __cplusplus
}
#endif
