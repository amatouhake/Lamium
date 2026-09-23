#include "features/inventory/game/ResponseBarrier.h"
#include "features/inventory/game/OwnedResponseBarrier.h"
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

    using namespace lamium::inventory::game;
    OwnedResponseBarrier owned;
    auto sort = owned.begin(now);
    check(sort.has_value(), "first operation owns response tracker");
    owned.track(*sort,-10);
    check(!owned.begin(now), "concurrent operation cannot reset in-flight requests");
    check(!owned.release(TransferToken{0}), "foreign cancellation cannot release the tracker");
    check(owned.result(*sort,now) == Result::Waiting, "failed acquisition preserves pending response");
    owned.respond(-10,true);
    check(owned.result(*sort,now) == Result::Accepted, "original operation receives its acknowledgement");
    check(!owned.begin(now), "completed operation retains ownership until explicitly released");
    check(owned.release(*sort), "owner can release the tracker");
    auto restock = owned.begin(now);
    check(restock && *restock != *sort, "next operation has a distinct token");
    owned.track(*restock,-11);
    owned.track(*sort,-12);
    check(!owned.release(*sort), "late old-job cancellation cannot cancel new work");
    check(owned.result(*sort,now) == Result::Untracked, "old token cannot read new operation results");
    owned.respond(-10,false);
    check(owned.result(*restock,now) == Result::Waiting, "old response cannot reject new work");
    owned.respond(-11,true);
    check(owned.result(*restock,now) == Result::Accepted, "old token cannot add unrelated requests");
    owned.reset();
    check(owned.result(*restock,now) == Result::Untracked, "shutdown invalidates ownership");
    auto restarted = owned.begin(now);
    check(restarted && *restarted != *restock, "restart never reuses outstanding tokens");
}
