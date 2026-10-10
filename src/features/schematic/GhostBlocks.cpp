#include "features/schematic/GhostRenderer.h"
#include "features/schematic/GhostCommon.h"
#include "features/schematic/GhostFaces.h"
#include "features/map/MapColors.h"
#include "app/Runtime.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/deps/minecraft_renderer/framebuilder/dragon/RenderMetadata.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/block/BlockTessellator.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/client/world/level/biome/biome_color_sampling/TessellationPolicy.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MeshData.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/biome/biome_color_sampling/BiomeColorSampling.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockRenderLayer.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/block/TintMethod.h"
#include "mc/world/level/block/VanillaStates.h"
#include "mc/world/level/material/Material.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <format>
#include <map>
#include <tuple>

namespace lamium::schematic::ghosts {
void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Schematic ghosts: {}", text); } catch (...) {}
}
// The palette entry as the NBT the game's block registry reads.
Block const* lookup(PaletteBlock const& entry) {
    nbt::Root root;
    root.compound.set("name", {entry.name});
    root.compound.set("states", {entry.states});
    root.compound.set("version", {entry.version});
    auto tag = CompoundTag::fromBinaryNbt(nbt::write(root));
    if (!tag) return nullptr;
    auto block = Block::tryGetFromRegistry(*tag);
    return block ? &*block : nullptr;
}
// A block's states as text, for naming what differs (the target card).
std::map<std::string, std::string> blockStates(Block const& block) {
    std::map<std::string, std::string> out;
    auto const& tags = block.mSerializationId->mTags;
    auto found = tags.find("states");
    if (found == tags.end()) return out;
    auto* states = std::get_if<CompoundTag>(&found->second.mTagStorage);
    if (!states) return out;
    for (auto const& [key, value] : states->mTags) {
        if (auto* v = std::get_if<ByteTag>(&value.mTagStorage)) out[key] = std::to_string(v->data);
        else if (auto* v = std::get_if<IntTag>(&value.mTagStorage)) out[key] = std::to_string(v->data);
        else if (auto* v = std::get_if<StringTag>(&value.mTagStorage)) out[key] = *v;
    }
    return out;
}
ItemInfo describe(Block const& block, std::string_view fallback) {
    ItemInfo out;
    auto item = block.getBlockType().asItemInstance(block, nullptr);
    if (item.isNull()) { out.name = std::string(fallback); return out; }
    out.item = item.getTypeName();
    out.name = item.getName();
    nbt::Root tag;
    tag.compound.set("Name", {out.item});
    tag.compound.set("Count", {std::int8_t{1}});
    tag.compound.set("Damage", {static_cast<std::int16_t>(item.getAuxValue())});
    out.icon = nbt::write(tag);
    return out;
}
bool blended(BlockRenderLayer layer) {
    return layer == BlockRenderLayer::RenderlayerBlend || layer == BlockRenderLayer::RenderlayerBlendToOpaque;
}

Block const* gameBlock(PaletteBlock const& entry) { return lookup(entry); }
BlockLabel blockLabel(PaletteBlock const& entry) {
    auto const* block = lookup(entry);
    auto info = block ? describe(*block, entry.name) : ItemInfo{"", entry.name, ""};
    return {info.name, info.icon};
}
void eachLayer(Block const& block, BlockSource& region, BlockPos const& pos, std::function<void(std::optional<BlockRenderLayer>)> const& visit) {
    // Liquids are drawn as shells (liquidShell), not through this path.
    if (block.getMaterial().mLiquid) { visit(std::nullopt); return; }
    auto const& type = block.getBlockType();
    auto own = type.getRenderLayer(block, region, pos);
    visit(own);
    // Extra layers as a bit per layer (the honey and slime blocks' other cube).
    int extra = type.getExtraRenderLayers();
    for (int layer = 0; layer < static_cast<int>(BlockRenderLayer::RenderlayerCount); ++layer)
        if ((extra >> layer & 1) && layer != static_cast<int>(own)) visit(static_cast<BlockRenderLayer>(layer));
}
void tessellateLayer(BlockTessellator& tessellator, Tessellator& batch, Block const& block, BlockPos const& pos,
                     std::optional<BlockRenderLayer> layer) {
    auto& current = static_cast<int&>(tessellator.mRenderingLayer);
    int was = current;
    if (layer) current = static_cast<int>(*layer);
    // A shape set by the previous block (the honey block's two cubes) must
    // not carry over: a slab drawn next drew full height.
    auto& shapeSet = static_cast<bool&>(tessellator.mCurrentShapeSet);
    shapeSet = false;
    size_t from = batch.mMeshData->mPositions->size();
    tessellator.tessellateInWorld(batch, block, pos, false);
    shapeSet = false;
    current = was;
    // Leaves come out of the tessellation gray: the world colors them in
    // its own shader (they stayed gray on the ghost and preview materials,
    // also when only the seasons layers were tinted). A block with a biome
    // tint method takes the tint the world's tessellation uses (as the
    // minimap), read at `pos`, on the vertices still gray. Only the foliage
    // methods (leaves, vines): grass tops come out tinted already, and
    // tinting a grass block's gray sides would turn its dirt green.
    auto tint = block.getBlockType().mTintMethod;
    if (tint != TintMethod::DefaultFoliage && tint != TintMethod::BirchFoliage && tint != TintMethod::EvergreenFoliage
        && tint != TintMethod::DryFoliage)
        return;
    auto value = BiomeColorSampling::getTessellationPolicy(tint).get(block, *static_cast<BlockSource*&>(tessellator.mRegion), pos, nullptr);
    static int logged = 0;
    if (logged < 6) {
        ++logged;
        log(std::format("tint {} for {} in layer {}: {:.2f} {:.2f} {:.2f}", static_cast<int>(tint), block.getTypeName(),
            layer ? static_cast<int>(*layer) : -1, value.r, value.g, value.b));
    }
    if (!map::usableTint(value.r, value.g, value.b)) return;
    auto& colors = batch.mMeshData->mColors.get();
    if (colors.size() != batch.mMeshData->mPositions->size()) return;
    auto scale = [](std::uint32_t c, int shift, float factor) {
        return static_cast<std::uint32_t>(std::lround(std::clamp(((c >> shift) & 255) * factor, 0.f, 255.f))) << shift;
    };
    for (size_t v = from; v < colors.size(); ++v) {
        auto& c = colors[v];
        int r = static_cast<int>(c & 255), g = static_cast<int>(c >> 8 & 255), b = static_cast<int>(c >> 16 & 255);
        if (std::abs(r - g) > 4 || std::abs(g - b) > 4) continue; // already tinted
        c = scale(c, 0, value.r) | scale(c, 8, value.g) | scale(c, 16, value.b) | (c & 0xff000000u);
    }
}
int liquidKind(Block const& block) {
    if (!block.getMaterial().mLiquid) return 0;
    auto type = block.getMaterial().mType;
    return type == SharedTypes::v1_26_20::MaterialType::Water ? 1 : type == SharedTypes::v1_26_20::MaterialType::Lava ? 2 : 0;
}
int liquidDepth(Block const& block) {
    return liquidKind(block) ? block.getState<int>(VanillaStates::LiquidDepth()).value_or(0) : 0;
}
bool liquidShell(BlockTessellator& tessellator, Tessellator& batch, BlockPos const& pos, Block const& liquid,
                 std::function<bool(int side)> const& open, std::function<float(int cx, int cz)> const& corner,
                 liquids::Flow flow) {
    // A white concrete cube tessellated at the cell gives quads with every
    // vertex stream the mesh needs; its faces are then reshaped, retextured
    // with the liquid's texture and recolored. The tessellator already
    // leaves out faces against opaque blocks.
    // Looked up each time: a block pointer kept across a registry reload
    // (changing a setting) crashed here.
    auto found = Block::tryGetFromRegistry(HashedString{"minecraft:white_concrete"});
    Block const* white = found ? &*found : nullptr;
    auto const* cubeGraphics = white ? BlockGraphics::getForBlock(*white) : nullptr;
    auto const* liquidGraphics = BlockGraphics::getForBlock(liquid);
    if (!white || !cubeGraphics || !liquidGraphics) return false;
    int kind = liquidKind(liquid);
    size_t from = batch.mMeshData->mPositions->size();
    tessellateLayer(tessellator, batch, *white, pos, std::nullopt);
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    auto& positions = *data.mPositions;
    auto& uvs = *data.mTextureUVs[0];
    auto& colors = *data.mColors;
    if (colors.size() != positions.size()) colors.resize(positions.size(), 0xffffffffu);
    // Water's texture is gray and tinted by the biome; lava's is colored,
    // and lava is not see-through.
    auto channel = [](float v, int shift) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.f, 1.f) * 255)) << shift; };
    std::uint32_t tint = kind == 1 ? channel(.25f, 0) | channel(.45f, 8) | channel(.95f, 16) | channel(.55f, 24)
                                   : channel(1.f, 0) | channel(1.f, 8) | channel(1.f, 16) | channel(1.f, 24);
    TextureUVCoordinateSet const& cube = cubeGraphics->getTexture(1, 0);
    float heights[2][2];
    for (int cx = 0; cx < 2; ++cx)
        for (int cz = 0; cz < 2; ++cz) heights[cx][cz] = corner(cx, cz);
    // Which way the liquid moves; a sloped edge of a still pool does not.
    glm::vec2 downhill{flow.x, flow.z};
    bool any = false;
    for (size_t q = from; q + 4 <= positions.size(); q += 4) {
        std::array<faces::Vertex, 4> quad;
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
        int side = faces::sideOf(quad, pos.x, pos.y, pos.z);
        if (side < 0 || !open(side)) {
            for (size_t k = 1; k < 4; ++k) positions[q + k] = positions[q];
            continue;
        }
        any = true;
        // Texture slots follow the faces: 0 down, 1 up (still), 2-5 the
        // sides (flowing). A moving liquid's top flows: the flowing texture
        // turned along the flow, one texture per block (scaled down only as far as a
        // diagonal turn needs to stay inside its atlas cell).
        bool flows = side == 3 && std::hypot(downhill.x, downhill.y) > 1e-4f;
        TextureUVCoordinateSet const& own = liquidGraphics->getTexture(side == 2 ? 0 : side == 3 && !flows ? 1 : 2, 0);
        for (size_t k = 0; k < 4; ++k) {
            // Top vertices go to their corner's height.
            auto& v = positions[q + k];
            float fx = v.x - static_cast<float>(pos.x), fz = v.z - static_cast<float>(pos.z);
            if (v.y > static_cast<float>(pos.y) + .5f) v.y = static_cast<float>(pos.y) + heights[fx > .5f][fz > .5f];
            colors[q + k] = tint;
            if (uvs.size() != positions.size()) continue;
            float u = 0, w = 0;
            if (flows) {
                // The texture's v axis along the flow.
                glm::vec2 along = glm::normalize(downhill), across{-along.y, along.x};
                glm::vec2 offset{fx - .5f, fz - .5f};
                float fit = 1.f / (std::abs(along.x) + std::abs(along.y));
                u = .5f + glm::dot(offset, across) * fit;
                w = .5f + glm::dot(offset, along) * fit;
            } else {
                float du = cube._u1 - cube._u0, dv = cube._v1 - cube._v0;
                u = du != 0 ? (uvs[q + k].x - cube._u0) / du : 0;
                w = dv != 0 ? (uvs[q + k].y - cube._v0) / dv : 0;
            }
            uvs[q + k] = {own._u0 + u * (own._u1 - own._u0), own._v0 + w * (own._v1 - own._v0)};
        }
    }
    static bool logged = false;
    if (!logged) {
        logged = true;
        auto const& still = liquidGraphics->getTexture(1, 0);
        log(std::format("liquid texture u {:.4f}-{:.4f} v {:.4f}-{:.4f} ({}x{}), cube u {:.4f}-{:.4f}", static_cast<float>(still._u0),
            static_cast<float>(still._u1), static_cast<float>(still._v0), static_cast<float>(still._v1),
            static_cast<int>(still._texSizeW), static_cast<int>(still._texSizeH), static_cast<float>(cube._u0), static_cast<float>(cube._u1)));
    }
    return any;
}
void reorderQuads(Tessellator& batch, std::vector<std::uint32_t> const& order) {
    auto& data = static_cast<mce::MeshData&>(batch.mMeshData);
    size_t quads = order.size();
    auto permute = [&](auto& stream) {
        if (stream.size() != quads * 4) return;
        auto copy = stream;
        for (size_t q = 0; q < quads; ++q)
            for (size_t k = 0; k < 4; ++k) stream[q * 4 + k] = copy[order[q] * 4 + k];
    };
    permute(*data.mPositions);
    permute(*data.mNormals);
    permute(*data.mTangents);
    permute(*data.mColors);
    permute(*data.mBoneId0s);
    for (int i = 0; i < 3; ++i) permute(*data.mTextureUVs[i]);
    permute(*data.mPBRTextureIndices);
    permute(*data.mMERS);
    permute(*data.mGeoType);
}
namespace {
// A block's quads, tessellated once above the build limit, where nothing
// culls them.
constexpr int probeY = 2000;
std::vector<std::array<faces::Vertex, 4>> probeQuads(Block const& block, BlockTessellator& tessellator, ScreenContext& screen, bool blendedToo) {
    Tessellator scratch(screen.tessellator.mBufferResourceService);
    scratch.begin({}, mce::PrimitiveMode::QuadList, 64, false);
    BlockPos above{0, probeY, 0};
    eachLayer(block, *static_cast<BlockSource*&>(tessellator.mRegion), above, [&](std::optional<BlockRenderLayer> layer) {
        if (blendedToo || !layer || !blended(*layer)) tessellateLayer(tessellator, scratch, block, above, layer);
    });
    auto const& positions = scratch.mMeshData->mPositions.get();
    std::vector<std::array<faces::Vertex, 4>> quads;
    for (size_t q = 0; q + 4 <= positions.size(); q += 4) {
        auto& quad = quads.emplace_back();
        for (size_t k = 0; k < 4; ++k) quad[k] = {positions[q + k].x, positions[q + k].y, positions[q + k].z};
    }
    scratch.end(Tessellator::UploadMode::Buffered, "Lamium schematic probe", SupplementaryFieldAutoGenerationMode{});
    return quads;
}
}
int sidesReached(Block const& block, BlockTessellator& tessellator, ScreenContext& screen, bool blendedToo, float epsilon) {
    static std::map<std::tuple<Block const*, bool, float>, int> known;
    auto key = std::tuple{&block, blendedToo, epsilon};
    if (auto found = known.find(key); found != known.end()) return found->second;
    int sides = 0;
    for (auto const& quad : probeQuads(block, tessellator, screen, blendedToo))
        if (int side = faces::sideOf(quad, 0, probeY, 0, epsilon); side >= 0) sides |= 1 << side;
    return known[key] = sides;
}
int sidesCovered(Block const& block, BlockTessellator& tessellator, ScreenContext& screen) {
    static std::map<Block const*, int> known;
    if (auto found = known.find(&block); found != known.end()) return found->second;
    std::array<float, 6> area{};
    for (auto const& quad : probeQuads(block, tessellator, screen, true))
        if (int side = faces::sideOf(quad, 0, probeY, 0); side >= 0) area[static_cast<size_t>(side)] += faces::sideArea(quad, side);
    int sides = 0;
    for (int side = 0; side < 6; ++side)
        if (faces::coveredBy(area[static_cast<size_t>(side)])) sides |= 1 << side;
    return known[&block] = sides;
}
bool coversNeighbors(Block const& block, BlockTessellator& tessellator, ScreenContext& screen) {
    // Only the layers drawn alpha-tested hide a face, and only where they
    // reach the side: a blended block (honey, whose opaque inner cube stops
    // short of the sides) shows what is behind it.
    return block.getBlockType().mIsOpaqueFullBlock && sidesReached(block, tessellator, screen, false, 1e-3f) == 63;
}
}
