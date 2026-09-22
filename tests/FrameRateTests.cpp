#include "features/information/FrameRateMeter.h"
void check(bool, char const*);
void frameRateTests() {
    lamium::information::FrameRateMeter meter;
    check(!meter.read(0), "frame statistics unavailable before samples");
    for (int i=0;i<=60;++i) meter.frame(i/60.);
    auto value = meter.read(1);
    check(value && std::abs(value->fps-60)<.001 && std::abs(value->milliseconds-1000./60)<.001,
          "60 Hz samples publish reciprocal FPS and mean interval");
    check(!meter.read(4), "stale frame measurements expire");
    meter.frame(4);
    check(!meter.read(4), "long suspension clears old published statistics");
    for (int i=1;i<=30;++i) meter.frame(4+i/30.);
    value = meter.read(5);
    check(value && std::abs(value->fps-30)<.001, "sampling recovers after suspension");
    meter.frame(4);
    check(!meter.read(4), "backwards timestamp resets window");
    meter.frame(4); meter.frame(4);
    check(!meter.read(4), "duplicate timestamps cannot invent frames");
    meter.reset();
    meter.frame(0); meter.frame(.1); meter.frame(.5);
    value = meter.read(.5);
    check(value && value->fps == 4 && value->milliseconds == 250,
          "uneven frame durations use total elapsed time, not mean instantaneous FPS");
}
