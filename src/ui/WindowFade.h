#pragma once
#include <algorithm>
#include <map>
#include <string>
// Lamium's parts beside or over a game window come and go with it where they
// stand, fading as the game's dark layer does, while the window slides in from
// below and away again (L-366). Where the window rests is its highest place
// seen. A screen seen before starts from the rest it had; one never seen is
// guessed centred, as the game places its windows. A window that stood still
// for 20 frames is at rest wherever it is, so a wrong guess or a rest from
// another window size cannot leave the parts faded.
namespace lamium::ui {
class WindowFade {
public:
    // Once a frame the window is drawn: its top and height, the screen's
    // height, the time in milliseconds. Returns where the window rests.
    float see(std::string const& screen, float y, float height, float screenHeight, double nowMs) {
        bool const reopened = !seen || nowMs - seenMs > 500;
        seen = true;
        seenMs = nowMs;
        auto known = rests.find(screen);
        if (reopened) {
            rest_ = std::min(known != rests.end() ? known->second : (screenHeight - height) / 2, y);
            still = 0;
        } else rest_ = std::min(rest_, y);
        still = !reopened && y == lastY ? still + 1 : 0;
        lastY = y;
        if (still >= 20) rest_ = y;
        if (rests.size() < 64 || known != rests.end()) rests[screen] = rest_;
        return rest_;
    }
    float rest() const { return rest_; }
    // How much of the parts shows: whole at rest, nothing once the window is
    // half the way from its rest to the screen's bottom.
    float opacity(float y, float screenHeight) const {
        float const slide = std::max(0.f, y - rest_), way = std::max(1.f, (screenHeight - rest_) * .5f);
        return std::clamp(1 - slide / way, 0.f, 1.f);
    }

private:
    std::map<std::string, float> rests;
    bool seen = false;
    double seenMs = 0;
    float rest_ = 0, lastY = 0;
    int still = 0;
};
}
