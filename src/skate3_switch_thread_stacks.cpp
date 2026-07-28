// Nintendo Switch: give every thread in the process a usable stack.
//
// devkitA64's pthread default stack is 1 MiB, and `std::thread` / `std::jthread`
// take it silently -- there is no portable way to ask for more. That is far too
// small for the deep C++ machinery this codebase runs off the main thread, and
// the failure is horrible to read: the stack runs off its region, a later `ret`
// pulls a garbage x30, and the crash surfaces as an Instruction Abort with the PC
// at the image base, in whatever function happened to return. Nothing points at
// the thread that actually overflowed.
//
// Four separate crashes were chased this way one thread at a time -- the renderer's
// decode workers, guest threads, the timer-queue dispatch loop -- each fixed by
// hand and each just moving the failure to the next 1 MiB thread. Every crashed
// thread's stack region measured exactly 1024 KiB.
//
// So fix it once, for everything. The linker's --wrap redirects every
// pthread_create in the image -- ours, libstdc++'s std::thread, and any library's
// -- through here, and a request that would have taken the small default gets a
// generous one instead. Callers that ask for their own size keep it, as long as it
// clears the floor.
//
// Deliberately NOT a per-call-site fix: the point is that a new std::thread added
// anywhere later cannot silently reintroduce this.

#if defined(__SWITCH__)

#include <pthread.h>

#include <cstddef>

namespace {

// Matches the renderer's decode workers, the proven value in this tree for a host
// thread running deep C++ machinery.
constexpr size_t kMinThreadStackBytes = 4u * 1024u * 1024u;

}  // namespace

extern "C" {

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start_routine)(void*), void* arg);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start_routine)(void*), void* arg) {
  pthread_attr_t local;
  bool local_initialized = false;

  if (attr == nullptr) {
    if (pthread_attr_init(&local) == 0) {
      local_initialized = true;
      pthread_attr_setstacksize(&local, kMinThreadStackBytes);
      attr = &local;
    }
  } else {
    size_t requested = 0;
    if (pthread_attr_getstacksize(attr, &requested) == 0 && requested < kMinThreadStackBytes) {
      // Copy rather than mutate: the caller owns `attr` and may reuse it.
      if (pthread_attr_init(&local) == 0) {
        local_initialized = true;
        // Preserve the one attribute that changes lifetime semantics; everything
        // else in this tree is left at the default.
        int detach_state = PTHREAD_CREATE_JOINABLE;
        pthread_attr_getdetachstate(attr, &detach_state);
        pthread_attr_setdetachstate(&local, detach_state);
        pthread_attr_setstacksize(&local, kMinThreadStackBytes);
        attr = &local;
      }
    }
  }

  int rc = __real_pthread_create(thread, attr, start_routine, arg);
  if (local_initialized) {
    pthread_attr_destroy(&local);
  }
  return rc;
}

}  // extern "C"

#endif  // __SWITCH__
