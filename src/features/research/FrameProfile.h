#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

// L-105 groundwork: the arithmetic of a profiler window (frame time, its
// 1 % lows and time per stage). No hook feeds it yet.
namespace lamium::research {
template <std::size_t Stages>
class FrameProfile {
public:
    struct Stage { double milliseconds = 0; double calls = 0; }; // Per frame, averaged.
    struct Summary {
        std::size_t frames = 0;
        double average = 0, worst = 0; // Milliseconds per frame.
        double low = 0;                // Mean of the slowest 1 % of frames, at least one frame.
        std::array<Stage, Stages> stages{};
        double averageFps() const { return average > 0 ? 1000 / average : 0; }
        double lowFps() const { return low > 0 ? 1000 / low : 0; }
    };
    explicit FrameProfile(std::size_t capacity = 4096) : capacity(std::max<std::size_t>(capacity, 1)) {
        frames.reserve(this->capacity);
    }
    // Time spent in one stage during the frame being measured.
    void stage(std::size_t index, double milliseconds) {
        if (index >= Stages || !std::isfinite(milliseconds) || milliseconds < 0) return;
        pending[index].milliseconds += milliseconds;
        pending[index].calls += 1;
    }
    // Ends the frame. A frame that is not a real interval (a pause, a clock
    // step) is dropped together with its stage times.
    void frame(double milliseconds) {
        bool const usable = std::isfinite(milliseconds) && milliseconds > 0 && milliseconds < 2000;
        if (usable && frames.size() < capacity) {
            frames.push_back(milliseconds);
            for (std::size_t i = 0; i < Stages; ++i) {
                totals[i].milliseconds += pending[i].milliseconds;
                totals[i].calls += pending[i].calls;
            }
        }
        pending = {};
    }
    bool full() const { return frames.size() >= capacity; }
    std::optional<Summary> summary() const {
        if (frames.empty()) return std::nullopt;
        Summary result;
        result.frames = frames.size();
        double sum = 0;
        for (double value : frames) { sum += value; result.worst = std::max(result.worst, value); }
        result.average = sum / static_cast<double>(frames.size());
        auto sorted = frames;
        std::size_t const slow = std::max<std::size_t>(frames.size() / 100, 1);
        std::partial_sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(slow), sorted.end(),
                          [](double a, double b) { return a > b; });
        double slowSum = 0;
        for (std::size_t i = 0; i < slow; ++i) slowSum += sorted[i];
        result.low = slowSum / static_cast<double>(slow);
        for (std::size_t i = 0; i < Stages; ++i)
            result.stages[i] = {totals[i].milliseconds / static_cast<double>(frames.size()),
                                totals[i].calls / static_cast<double>(frames.size())};
        return result;
    }
    void reset() { frames.clear(); totals = {}; pending = {}; }

private:
    std::size_t capacity;
    std::vector<double> frames;
    std::array<Stage, Stages> totals{}, pending{};
};
}
