// dlfcn.h - Nintendo Switch / newlib stub
// The Switch has no dynamic linker. Volk (and similar libs) include this on
// non-Windows targets. We provide no-op stubs so they compile; at runtime
// volk will call volkInitializeCustom() which is the real entry point.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY   0x00001
#define RTLD_NOW    0x00002
#define RTLD_GLOBAL 0x00100
#define RTLD_LOCAL  0x00000
#define RTLD_NODELETE 0x01000
#define RTLD_NOLOAD 0x00004
#define RTLD_DEEPBIND 0x00008

// Pseudo-handles used by dlsym() on some platforms. With no dynamic linker these
// are inert; dlsym() always returns null.
#define RTLD_DEFAULT ((void *)0)
#define RTLD_NEXT    ((void *)-1L)

static inline void *dlopen(const char *filename, int flags) {
    (void)filename; (void)flags; return (void *)0;
}
static inline void *dlsym(void *handle, const char *name) {
    (void)handle; (void)name; return (void *)0;
}
static inline int dlclose(void *handle) {
    (void)handle; return 0;
}
static inline char *dlerror(void) {
    return (char *)0;
}

#ifdef __cplusplus
}
#endif
