#pragma once
#include "features/information/PaperDoll.h"
#include <optional>
#include <string_view>

namespace lamium::information::debugVanilla {
// While Debug View is open, the game's own position line and paper doll are
// not drawn, so they do not sit under it (L-143); each also while the Info
// HUD covers it (L-145). The game's options stay as they are.
// Names from the vanilla hud_screen: the position panel, its label, and the
// paper doll's custom control (drawn by HudPlayerRenderer).
inline bool positionControl(std::string_view name) {
    return name == "player_position" || name == "player_position_text";
}
// Set each frame by the HUD draw.
void infoCovers(bool doll, bool position);
// Where the paper doll's control is centered while Paper doll position is on
// (L-309), in HUD units, and how large it is drawn (1 is the game's size);
// nothing leaves the doll as the game draws it. Set each frame by the HUD draw.
void placeDoll(std::optional<paperDoll::Point> center, float scale = 1);
void start() noexcept;
void stop();
}
