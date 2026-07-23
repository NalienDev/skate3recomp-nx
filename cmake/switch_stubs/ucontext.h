// ucontext.h - Nintendo Switch / devkitA64 (newlib) shim
//
// newlib on devkitA64 does not ship <ucontext.h>. The rexglue runtime uses the
// System V ucontext API only for cooperative fibers (getcontext/makecontext/
// swapcontext) in fiber_posix.cpp, and includes the header (without touching
// uc_mcontext) in a couple of POSIX signal files. This provides a minimal,
// self-contained ucontext_t plus an AArch64 implementation of the four context
// primitives (see src/switch/ucontext_aarch64.c in the skate3 project).
//
// Only callee-saved state is preserved (x19-x30, sp, d8-d15), which is all a
// cooperative context switch through a function-call boundary requires. TLS
// (TPIDR_EL0) is intentionally NOT swapped: every fiber runs on the same host
// thread, so thread_local storage must remain shared across fibers.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void*  ss_sp;
  size_t ss_size;
  int    ss_flags;
} __rex_stack_t;

// Layout is depended upon by the assembly in ucontext_aarch64.c:
//   __regs[0..11] = x19,x20,...,x28,x29(fp),x30(lr)   byte offset 0
//   __sp                                              byte offset 96
//   __fpregs[0..7] = d8..d15                          byte offset 104
typedef struct __rex_ucontext {
  uint64_t __regs[12];
  uint64_t __sp;
  uint64_t __fpregs[8];
  __rex_stack_t uc_stack;
  struct __rex_ucontext* uc_link;
} ucontext_t;

// Provide mcontext_t so headers that reference the type still parse. It carries
// no meaningful host-register state on this target (fault context is unused).
typedef struct {
  uint64_t __unused_regs[32];
} mcontext_t;

int  getcontext(ucontext_t* ucp);
int  setcontext(const ucontext_t* ucp);
void makecontext(ucontext_t* ucp, void (*func)(void), int argc, ...);
int  swapcontext(ucontext_t* oucp, const ucontext_t* ucp);

#ifdef __cplusplus
}
#endif
