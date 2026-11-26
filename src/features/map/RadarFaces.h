#pragma once
#include "features/map/MapFaces.h"
class Actor;
class SerializedSkinRef;
class IClientInstance;
class MinecraftUIRenderContext;
namespace lamium::map::faces {
// Mob faces for the radar (BACKLOG L-85), found once per kind of mob from
// its renderer's model ("head" part, front face) and texture. Client
// thread only.
// The face for this actor's kind, or -1 when it has none (or is still
// waiting for this frame's load budget).
int faceOf(IClientInstance&, Actor&);
// The head from this player's skin (L-87), by skin, in the same atlas, or
// -1 (a skin in another layout stays a dot).
int headOf(Actor& player);
// The same from a skin without its player, as the player list holds it (L-89).
int headOf(SerializedSkinRef const& skin);
// Each frame before looking faces up: a few new kinds load per frame.
void frame();
// Research builds only (radar_icon_probe, L-330): once per world, every mob kind's face to logs/radar-sweep.*.
void sweep(IClientInstance&);
// Draws a face centered on (x, y) in GUI units, its longer side about
// `size` units, each texel a whole number of screen pixels, inside a black
// ring. False when it could not be drawn (the caller draws a dot).
bool draw(MinecraftUIRenderContext&, int index, float x, float y, float size, float alpha);
// A new world may bring other resource packs.
void forget(IClientInstance*);
}
