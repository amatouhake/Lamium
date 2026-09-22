#pragma once
#include <chrono>
#include <cstdint>
#include <map>

namespace lamium::inventory::game {
// One vanilla transfer may emit several requests. All of its own requests
// must succeed; unrelated responses never advance the operation.
class ResponseBarrier {
public:
    using Clock = std::chrono::steady_clock;
    enum class Result { Waiting, Accepted, Rejected, Untracked, TimedOut };
    void begin(Clock::time_point now) { requests.clear(); failed = false; started = now; }
    void track(int64_t id) { requests.try_emplace(id, false); }
    void respond(int64_t id, bool accepted) {
        auto it = requests.find(id);
        if (it == requests.end()) return;
        if (!accepted) failed = true;
        it->second = true;
    }
    Result result(Clock::time_point now) const {
        if (failed) return Result::Rejected;
        if (requests.empty()) return Result::Untracked;
        for (auto const& [id, received] : requests) {
            if (!received) return now-started >= std::chrono::seconds(5) ? Result::TimedOut : Result::Waiting;
        }
        return Result::Accepted;
    }
private:
    std::map<int64_t, bool> requests;
    bool failed = false;
    Clock::time_point started;
};
}
