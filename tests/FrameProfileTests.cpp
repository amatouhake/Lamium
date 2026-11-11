#include "features/research/FrameProfile.h"
#include <cmath>
void check(bool, char const*);
void frameProfileTests() {
    using lamium::research::FrameProfile;
    auto near = [](double a, double b) { return std::abs(a - b) < 1e-9; };
    FrameProfile<2> profile(300);
    check(!profile.summary(), "no summary before a frame");
    for (int i = 0; i < 198; ++i) {
        profile.stage(0, 2);
        profile.stage(1, 0.5);
        profile.stage(1, 0.5);
        profile.frame(10);
    }
    profile.stage(0, 20);
    profile.frame(40);
    profile.frame(60);
    auto summary = profile.summary();
    check(summary && summary->frames == 200 && near(summary->average, (198 * 10 + 100) / 200.0),
          "the average covers every frame");
    check(near(summary->worst, 60) && near(summary->low, 50) && near(summary->lowFps(), 20),
          "the 1 % low is the mean of the slowest hundredth of the frames");
    check(near(summary->stages[0].milliseconds, (198 * 2 + 20) / 200.0) && near(summary->stages[0].calls, 199 / 200.0),
          "a stage reports its time and calls per frame");
    check(near(summary->stages[1].milliseconds, 198 / 200.0) && near(summary->stages[1].calls, 396 / 200.0),
          "several calls in one frame add up");

    FrameProfile<1> few;
    few.frame(5);
    few.frame(15);
    check(near(few.summary()->low, 15) && near(few.summary()->averageFps(), 100), "under 100 frames the low is the slowest frame");

    FrameProfile<1> dropped(4);
    dropped.stage(0, 9);
    dropped.frame(5000);
    dropped.stage(0, 9);
    dropped.frame(-1);
    dropped.stage(3, 1);
    dropped.stage(0, std::nan(""));
    dropped.frame(8);
    check(dropped.summary()->frames == 1 && near(dropped.summary()->stages[0].milliseconds, 0),
          "a pause or clock step is dropped with its stage times, and bad stage input is ignored");
    for (int i = 0; i < 10; ++i) dropped.frame(8);
    check(dropped.full() && dropped.summary()->frames == 4, "the window stops at its capacity");
    dropped.reset();
    check(!dropped.summary() && !dropped.full(), "reset empties the window");
}
