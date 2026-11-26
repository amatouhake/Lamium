#pragma once
// L-353 (thread_trace): which thread the client tick, UI render, key and mouse
// events, focus loss, world exit and Runtime::disable run on. Each place logs
// its first three calls, so the log answers the question in one session.
#ifdef LAMIUM_THREAD_TRACE
#include "app/Runtime.h"
#include <atomic>
#include <functional>
#include <thread>
namespace lamium::thread_trace {
inline void note(char const* where, std::atomic<int>& seen) noexcept {
    int const n = seen.fetch_add(1);
    if (n >= 3) return;
    try {
        Runtime::instance().self().getLogger().info("Thread trace (L-353): {} #{} on thread {}", where, n + 1,
            std::hash<std::thread::id>{}(std::this_thread::get_id()));
    } catch (...) {}
}
}
#define LAMIUM_NOTE_THREAD(where) \
    do { static std::atomic<int> lamiumThreadSeen{0}; ::lamium::thread_trace::note(where, lamiumThreadSeen); } while (0)
#else
#define LAMIUM_NOTE_THREAD(where) do {} while (0)
#endif
