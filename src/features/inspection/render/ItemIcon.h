#pragma once
#include <span>
class ItemStack;
class MinecraftUIRenderContext;
namespace lamium::inspection::render {
// Item icons drawn the way vanilla inventory slots draw them (L-91, L-119).
// Slots add block and flat items to a shared-mesh batch that the UI draws
// with its item material; drawn on their own, fence gates show nothing and
// leather loses its undyeable layer. Other items (shields, entity blocks)
// and glinting items keep the item renderer's direct path. With `glint`, a
// glinting item gets the glint overlay right after its own icon: the foil
// call covers the icon drawn just before it, so drawn later (after other
// icons) it showed nothing (L-142).
struct IconAt {
    ItemStack const* stack;
    float x, y;
    float scale = 1.f; // 1 draws a 16x16 icon
    int frame = 0;
    float alpha = 1.f; // Whole-icon opacity (a faded player list dimension, L-131)
    bool glint = false; // Draw the glint of items that have one (Item::isGlint)
    // Draw it on its own, in its place among the others, instead of in a shared batch: batches are drawn after
    // every glinting icon, so an icon that must be on top of batched ones asks for this (L-170).
    bool direct = false;
};
void drawItemIcons(MinecraftUIRenderContext& context, std::span<IconAt const> icons, int zOrder);
inline void drawItemIcon(MinecraftUIRenderContext& context, IconAt const& icon, int zOrder) {
    drawItemIcons(context, std::span<IconAt const>{&icon, 1}, zOrder);
}
}
