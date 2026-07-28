// Nintendo Switch: record WHY the process died.
//
// Runs have ended with the app simply gone: the log stops mid-frame, Atmosphère's
// report names a thread that was merely asleep, and none of the paths that are
// supposed to explain an exit -- FatalError, the applet pump's deliberate
// termination, a GPU fault -- left a line behind. Without knowing which exit was
// taken, every such crash is a guess.
//
// So every way out of this process announces itself first, through REX_BOOTLOG
// (open/write/flush/close per call, so the line survives whatever happens next).
//
// The linker's --wrap is used rather than per-call-site logging for the same
// reason as the thread-stack wrapper: these are reached from libc, libstdc++ and
// libnx as well as from our own code, and a call added later must not be able to
// slip out silently.

#if defined(__SWITCH__)

#include <switch.h>

#include <cstdlib>
#include <exception>

#include <rex/bootlog_switch.h>

extern "C" {

void __real_abort(void) __attribute__((noreturn));
void __real_exit(int status) __attribute__((noreturn));
void __real__exit(int status) __attribute__((noreturn));
void __real_diagAbortWithResult(Result res) __attribute__((noreturn));

void __wrap_abort(void) {
  // rex::FatalError ends here, as do libstdc++ assertions and std::terminate's
  // default handler. Its message has already gone to stderr, which is redirected
  // to sdmc:/switch/skate3/stdio.log.
  REX_BOOTLOG("EXIT: abort() -- see stdio.log for the message");
  __real_abort();
}

void __wrap_exit(int status) {
  REX_BOOTLOG("EXIT: exit(%d)", status);
  __real_exit(status);
}

void __wrap__exit(int status) {
  REX_BOOTLOG("EXIT: _exit(%d)", status);
  __real__exit(status);
}

void __wrap_diagAbortWithResult(Result res) {
  // libnx's own panic. This is what fires when a libnx API is misused -- the HID
  // abort that killed the intro video came through here.
  REX_BOOTLOG("EXIT: diagAbortWithResult(0x%x) -- libnx panic", (unsigned)res);
  __real_diagAbortWithResult(res);
}

}  // extern "C"

namespace {

void OnTerminate() {
  // Reached for an unhandled exception, or a throw during unwinding. Try to name
  // the exception: that is usually the whole answer, and it is lost otherwise.
  const char* what = "unknown";
  if (auto eptr = std::current_exception()) {
    try {
      std::rethrow_exception(eptr);
    } catch (const std::exception& e) {
      what = e.what();
    } catch (...) {
      what = "non-std exception";
    }
  }
  REX_BOOTLOG("EXIT: std::terminate -- unhandled exception: %s", what);
  __real_abort();
}

struct InstallExitTrace {
  InstallExitTrace() {
    std::set_terminate(&OnTerminate);
    REX_BOOTLOG("exit trace installed");
  }
};

// Static init order does not matter here: every hook is either a linker wrap
// (always live) or set_terminate, which only needs to beat the first throw.
InstallExitTrace g_install_exit_trace;

}  // namespace

#endif  // __SWITCH__
