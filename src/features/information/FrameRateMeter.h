#pragma once
#include <cmath>
#include <optional>
namespace lamium::information {
struct FrameStatistics { double fps{}, milliseconds{}; };
// Samples frame completion cadence, not GPU timings or simulation ticks.
class FrameRateMeter {
    std::optional<double> last;
    double elapsed = 0;
    unsigned intervals = 0;
    std::optional<FrameStatistics> published;
public:
    void reset() { last.reset(); elapsed = 0; intervals = 0; published.reset(); }
    void frame(double seconds) {
        if (!std::isfinite(seconds)) { reset(); return; }
        if (!last) { last = seconds; return; }
        double delta = seconds-*last;
        if (delta < 0 || delta > 2) { reset(); last = seconds; return; }
        if (delta == 0) return;
        last = seconds;
        elapsed += delta;
        ++intervals;
        if (elapsed >= .5) {
            published = FrameStatistics{intervals/elapsed,elapsed*1000/intervals};
            elapsed = 0; intervals = 0;
        }
    }
    std::optional<FrameStatistics> read(double now) const {
        if (!last || !std::isfinite(now) || now < *last || now-*last > 2) return {};
        return published;
    }
};
}
