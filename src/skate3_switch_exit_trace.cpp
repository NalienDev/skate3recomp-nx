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
void __real_svcExitProcess(void) __attribute__((noreturn));

void __wrap_abort(void) {
  // rex::FatalError ends here, as do libstdc++ assertions and std::terminate's
  // default handler. Its message has already gone to stderr, which is redirected
  // to sdmc:/switch/skate3/stdio.log.
  //
  // Log the CALLER too. An abort with no message on stderr says almost nothing --
  // and that is exactly what happened: something aborted silently and the only
  // visible damage was an unrelated thread faulting as the image unmapped.
  // Symbolicate with:
  //   aarch64-none-elf-addr2line -f -C -e skate3.elf <caller - slide>
  REX_BOOTLOG("EXIT: abort() from %p -- see stdio.log for any message",
              __builtin_return_address(0));
  __real_abort();
}

void __wrap_exit(int status) {
  REX_BOOTLOG("EXIT: exit(%d) from %p", status, __builtin_return_address(0));
  __real_exit(status);
}

void __wrap__exit(int status) {
  REX_BOOTLOG("EXIT: _exit(%d) from %p", status, __builtin_return_address(0));
  __real__exit(status);
}

void __wrap_diagAbortWithResult(Result res) {
  // libnx's own panic. This is what fires when a libnx API is misused -- the HID
  // abort that killed the intro video came through here.
  REX_BOOTLOG("EXIT: diagAbortWithResult(0x%x) from %p -- libnx panic", (unsigned)res,
              __builtin_return_address(0));
  __real_diagAbortWithResult(res);
}

void __wrap_svcExitProcess(void) {
  // The last hole in this trace, and the one that fits the runs we cannot
  // explain: several runs have ended with NO Atmosphere crash report, no EXIT
  // line, no WATCHDOG line and no applet-terminate line, while the last logged
  // frame looked perfectly healthy (29.6 fps). No report means no CPU fault, and
  // Horizon still returned cleanly to hbmenu -- which is what svcExitProcess
  // does. Every other way out already announces itself, so if something is
  // calling this directly (libnx internals, or a path that never reaches
  // exit()/abort()), this is the only thing that can name it.
  //
  // Our own applet pump calls this deliberately, but it logs
  // "APPLET ...; terminating" first, so its line is already distinguishable.
  //
  // If a silent death produces NO line here either, the process is being killed
  // from outside and no in-process instrumentation will ever catch it -- which is
  // itself the answer, and would end that line of investigation.
  REX_BOOTLOG("EXIT: svcExitProcess() from %p", __builtin_return_address(0));
  __real_svcExitProcess();
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
