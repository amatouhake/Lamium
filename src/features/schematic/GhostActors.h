#pragma once
// Ghost block actors (chests, beds, banners, heads) lit fully bright: the
// hooks that replace the light their renderers set up while a ghost's block
// source draws (makeBlockActor and renderBlockActor in GhostRenderer.h).
namespace lamium::schematic::ghosts {
// False when either light hook could not be installed.
bool startActorLight();
void stopActorLight();
}
