#include "features/inventory/game/ResponseBarrier.h"
void check(bool, char const*);
void responseBarrierTests() {
    using Barrier = lamium::inventory::game::ResponseBarrier;
    using Result = Barrier::Result;
    Barrier barrier;
    auto now = Barrier::Clock::now();
    barrier.begin(now);
    check(barrier.result(now) == Result::Untracked, "no captured request cannot count as acknowledgement");
    barrier.track(-1);
    barrier.track(-2);
    barrier.respond(-99, false);
    check(barrier.result(now) == Result::Waiting, "unrelated rejection does not affect transfer");
    barrier.respond(-1, true);
    barrier.track(-1);
    check(barrier.result(now) == Result::Waiting, "all transfer requests must be acknowledged");
    barrier.respond(-2, true);
    check(barrier.result(now) == Result::Accepted, "matching approvals release barrier");
    barrier.begin(now);
    barrier.track(-3);
    barrier.respond(-1, true);
    check(barrier.result(now) == Result::Waiting, "old response cannot approve next transfer");
    barrier.respond(-3, false);
    barrier.respond(-3, true);
    check(barrier.result(now) == Result::Rejected, "rejection cannot be erased by duplicate approval");
    barrier.begin(now);
    barrier.track(-4);
    check(barrier.result(now + std::chrono::seconds(4)) == Result::Waiting, "wait within deadline");
    check(barrier.result(now + std::chrono::seconds(5)) == Result::TimedOut, "missing acknowledgement times out");
}
