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
#include <rex/bootlog_switch.h>

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
}

struct SwitchInitHelper {
  SwitchInitHelper() {
    // Crash-proof early logging: reopened and flushed per line, so it survives
    // the process dying. It is the only diagnostic channel available before
    // the real logger exists, and on a console with no stdout.
    rex::bootlog::Begin();
    romfsInit();
    socketInitializeDefault();
  }
  ~SwitchInitHelper() {
    socketExit();
    romfsExit();
  }
};
static SwitchInitHelper g_switch_init;
#endif

REX_DEFINE_APP(skate3, Skate3PureApp::Create)
