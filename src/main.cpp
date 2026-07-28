// skate3 - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.

#include "skate3_app_common.h"
#include <skate3_version.h>

#include <string>
#include <string_view>

// NVIDIA Optimus / AMD PowerXpress discrete-GPU opt-in. The drivers only read
// these from the executable's export table; the copies in rexruntime.dll are
// invisible to them, which left hybrid-graphics laptops rendering on the
// integrated GPU. They must live here, in the exe image itself.
#if defined(_WIN32)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) unsigned long AmdPowerXpressRequestHighPerformance = 1;
}
#endif

class Skate3PureApp : public Skate3BaseApp {
 public:
  using Skate3BaseApp::Skate3BaseApp;

  std::string_view GetBuildTitle() const override {
    return SKATE3_BUILD_TITLE;
  }

  std::string_view GetBuildStamp() const override {
    return SKATE3_BUILD_STAMP;
  }

  std::string GetWindowTitle() const override {
#if defined(__APPLE__)
    // Match the macOS bundle name so the title bar and the .app agree.
    return "skate3recomp " SKATE3_BUILD_TITLE;
#else
    return "Skate 3 " SKATE3_BUILD_TITLE;
#endif
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Skate3PureApp>(
        new Skate3PureApp(ctx, "skate3", skate3_PPCImageConfig));
  }
};

#if defined(__SWITCH__)
#include <switch.h>
#include <pthread.h>

#include <atomic>
#include <cstdio>

#include <rex/bootlog_switch.h>
#include <rex/switch_applet.h>

// libnx runtime configuration, overriding the weak defaults in the executable
// image itself (weak symbols in a linked library are invisible to libnx's crt0,
// exactly like NvOptimusEnablement above). Mesa NVK brings the GPU up through
// the nv service and a GPFIFO channel during Vulkan init; that allocation
// exceeds the default LibraryApplet memory pool, so vkCreateInstance fails with
// VK_ERROR_INITIALIZATION_FAILED. Running as a full Application with the whole
// memory pool (heap_size 0 => libnx takes all that remains) is the configuration
// every NVK-on-Switch reference app uses.
extern "C" {
u32 __nx_applet_type = AppletType_Application;
size_t __nx_heap_size = 0;

// Mesa drm_shim's log sink hook (see the note where it is assigned).
extern void (*g_drm_shim_log_sink)(const char*);
}

// Applet lifecycle. Horizon expects an application to pump the applet message
// queue; that is how it learns the app has acknowledged losing focus so the
// system can take the display for the HOME menu or a sleep transition.
//
// Nothing here did that, and the consequence is not a dead app -- it is a dead
// CONSOLE. Pressing HOME made the compositor wait forever for a surface this
// process never yielded, so the whole system appeared frozen and only a
// power-button hold recovered it.
//
// A dedicated thread owns the pump, because it must keep running even when the
// render or guest threads are wedged (a hung Vulkan present is exactly when the
// user reaches for HOME). When the system asks us to exit, the recompiled guest
// has no clean shutdown path to run -- a dozen guest threads sit in kernel waits
// and the timer queue's dispatch thread would execute unmapped code once libnx
// hands the NRO's memory back. So the flag is published for anyone who can act
// on it, the log is flushed, and then the process is taken down at the kernel
// level with svcExitProcess. That cannot hang, and it guarantees the kernel
// reclaims the display and hbloader comes back instead of the console wedging.
namespace {

std::atomic<bool> g_applet_pump_running{false};
std::atomic<bool> g_applet_exit_requested{false};
pthread_t g_applet_pump_thread{};
bool g_applet_pump_started = false;

[[noreturn]] void AppletTerminate(const char* why) {
  g_applet_exit_requested.store(true, std::memory_order_release);
  // REX_BOOTLOG opens/flushes/closes per call, so the line is already on the card
  // by the time this returns.
  REX_BOOTLOG("APPLET %s; terminating", why);
  svcSleepThread(150000000ULL);  // 150 ms so an in-flight log line lands
  svcExitProcess();
  __builtin_unreachable();
}

void* AppletPumpMain(void*) {
  // Time spent continuously out of focus before the process is taken down.
  constexpr uint64_t kFocusGraceNs = 2000000000ULL;  // 2 s
  uint64_t out_of_focus_since = 0;
  AppletFocusState last_state = AppletFocusState_InFocus;

  while (g_applet_pump_running.load(std::memory_order_relaxed)) {
    // An ExitRequested message is the clean path, but pressing HOME on an
    // Application does not send one -- the system simply takes FOCUS and waits
    // for the app to stop drawing. Nothing here can make the renderer stop: the
    // guest drives it, and it may be wedged in NVK. Left alone the compositor
    // waits on a surface it cannot reclaim and the whole console locks up, which
    // is what a HOME press did.
    if (!appletMainLoop()) {
      AppletTerminate("exit requested by the system");
    }

    const AppletFocusState state = appletGetFocusState();
    if (state != last_state) {
      REX_BOOTLOG("APPLET focus state -> %d", (int)state);
      last_state = state;
    }

    // A library applet we opened ourselves (the software keyboard) takes focus by
    // definition. Terminating then would kill the app the instant the keyboard
    // appeared -- and the keyboard is the only way past Skate 3's team-name
    // prompt.
    if (state == AppletFocusState_InFocus || rex::switch_applet::LibraryAppletOpen()) {
      out_of_focus_since = 0;
    } else {
      const uint64_t now = armGetSystemTick();
      if (out_of_focus_since == 0) {
        out_of_focus_since = now;
      } else if (armTicksToNs(now - out_of_focus_since) >= kFocusGraceNs) {
        // Quitting to hbmenu is a far better outcome than a console that needs a
        // power-button hold. Revisit if the renderer ever learns to idle on focus
        // loss and hand the surface back.
        AppletTerminate("out of focus past the grace period");
      }
    }

    svcSleepThread(16666667ULL);  // ~60 Hz
  }
  return nullptr;
}

void StartAppletPump() {
  g_applet_pump_running.store(true, std::memory_order_relaxed);
  // Explicit stack size: newlib gives raw threads a small default, and this
  // thread must be the one that never dies.
  pthread_attr_t attr;
  if (pthread_attr_init(&attr) != 0) {
    REX_BOOTLOG("APPLET pump: pthread_attr_init failed; HOME will not be safe");
    return;
  }
  pthread_attr_setstacksize(&attr, 64u * 1024u);
  int rc = pthread_create(&g_applet_pump_thread, &attr, AppletPumpMain, nullptr);
  pthread_attr_destroy(&attr);
  if (rc != 0) {
    g_applet_pump_running.store(false, std::memory_order_relaxed);
    REX_BOOTLOG("APPLET pump: pthread_create failed rc=%d; HOME will not be safe", rc);
    return;
  }
  g_applet_pump_started = true;
  REX_BOOTLOG("APPLET pump started");
}

}  // namespace

// True once the system has asked the application to exit.
extern "C" bool skate3_switch_exit_requested() {
  return g_applet_exit_requested.load(std::memory_order_acquire);
}

struct SwitchInitHelper {
  SwitchInitHelper() {
    // Crash-proof early logging: reopened and flushed per line, so it survives
    // the process dying. It is the only diagnostic channel available before
    // the real logger exists, and on a console with no stdout.
    rex::bootlog::Begin();
    romfsInit();

    // Give stdio somewhere real to go BEFORE anything else can use it.
    //
    // Horizon hands a homebrew process no stdout or stderr, and newlib's FILE
    // objects for them are not usable as-is. That is fine until a library
    // decides to log: Mesa's DRM shim calls shim_log() from inside
    // nvk_queue_submit when it meets something it does not like, which reached
    // _write_r through fflush and took a Data Abort on a null FILE -- the app died
    // in the middle of a Vulkan present, on the main menu.
    //
    // Pointing both at a file makes any such log harmless, and captures whatever
    // the driver was trying to say, which is the more interesting half.
    if (!std::freopen("sdmc:/switch/skate3/stdio.log", "w", stderr)) {
      REX_BOOTLOG("stdio: freopen(stderr) failed");
    }
    if (!std::freopen("sdmc:/switch/skate3/stdio.log", "a", stdout)) {
      REX_BOOTLOG("stdio: freopen(stdout) failed");
    }
    // Line buffering: a crash must not eat the line that explains it.
    std::setvbuf(stderr, nullptr, _IOLBF, 0);
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    // Silence Mesa's DRM shim.
    //
    // The NVK library we link was built WITH DRM_SHIM_DEBUG (shim_log is present
    // in libvulkan.a), so drm_shim traces EVERY ioctl -- and its default sink
    // fopen()s sdmc:/nvk_drmshim.log and fflush()es per line. That is a
    // synchronous SD-card write for every GPU operation, which is a large part of
    // why the game runs at single-digit frame rates, and it is what faulted in
    // shim_log on a null FILE at the main menu.
    //
    // The shim exports a sink hook that short-circuits all of that, so point it at
    // a discard. This is a mitigation, not the fix: the real repair is rebuilding
    // the NVK library without DRM_SHIM_DEBUG, which also removes the pushbuf
    // "peek" that dereferences a BO's CPU mapping on every push whether or not
    // logging is on -- that wild read is the nouveau_dispatch crash, and it cannot
    // be disabled from here because the arguments are evaluated before shim_log
    // is ever called.
    g_drm_shim_log_sink = [](const char*) {};
    REX_BOOTLOG("drm_shim: log sink discarded (built with DRM_SHIM_DEBUG)");

    socketInitializeDefault();
    StartAppletPump();
  }
  ~SwitchInitHelper() {
    g_applet_pump_running.store(false, std::memory_order_relaxed);
    if (g_applet_pump_started) {
      pthread_join(g_applet_pump_thread, nullptr);
      g_applet_pump_started = false;
    }
    socketExit();
    romfsExit();
  }
};
static SwitchInitHelper g_switch_init;
#endif

REX_DEFINE_APP(skate3, Skate3PureApp::Create)
