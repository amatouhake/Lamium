#include "overlay/WorldOverlay.h"
#include "overlay/ChunkBorders.h"
#include "overlay/Hitboxes.h"
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/client/renderer/BaseActorRenderContext.h"
#include "mc/client/renderer/Tessellator.h"
#include "mc/client/renderer/RenderMaterialGroup.h"
#include "mc/client/renderer/SupplementaryFieldAutoGenerationMode.h"
#include "mc/client/gui/screens/ScreenContext.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/Level.h"
#include "mc/deps/core_graphics/enums/PrimitiveMode.h"
#include "mc/deps/minecraft_renderer/renderer/Mesh.h"
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"
#include "mc/deps/minecraft_renderer/renderer/TexturePtr.h"
#include "mc/deps/minecraft_renderer/resources/ClientTexture.h"
#include "mc/deps/minecraft_renderer/resources/ServerTexture.h"
#include "mc/deps/minecraft_renderer/resources/OffscreenCaptureDescription.h"
#include <span>

namespace lamium::overlay {
namespace {
bool installed = false;
void drawLines(BaseActorRenderContext& context, std::span<Line const> lines, bool hitboxes = false) {
    if (lines.empty() || !context.mImpl) return;
    ScreenContext& screen = context.mScreenContext;
    Tessellator& shared = screen.tessellator;
    // Own the temporary tessellation state. Never reset or reuse a partially
    // assembled vanilla batch. Mesh lifetime follows the engine submission API.
    Tessellator batch(shared.mBufferResourceService);
    batch.begin({}, mce::PrimitiveMode::LineList, static_cast<int>(lines.size()*2), false);
    if (hitboxes) batch.color(1.f,1.f,1.f,1.f);
    else batch.color(.2f,.85f,1.f,1.f);
    Vec3 const camera = context.mImpl->mCameraPosition;
    for (auto const& line : lines) for (auto p : {line.from, line.to})
        batch.vertex(static_cast<float>(p.x-camera.x), static_cast<float>(p.y-camera.y), static_cast<float>(p.z-camera.z));
    auto mesh = batch.end(Tessellator::UploadMode::Buffered, "Lamium world lines", SupplementaryFieldAutoGenerationMode{});
    mce::MaterialPtr material(mce::RenderMaterialGroup::common(), HashedString{"debug"});
    if (!material.mRenderMaterialInfoPtr) return;
    mesh.renderMesh(screen, material, gsl::span<mce::ClientTexture const*>{}, 0,
        static_cast<uint>(lines.size()*2), OffscreenCaptureDescription{}, nullptr);
}
LL_TYPE_INSTANCE_HOOK(WorldLines, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$renderEntityEffects, void, BaseActorRenderContext& context) {
    origin(context);
    auto& runtime = Runtime::instance();
    if (!runtime.enabled()) return;
    auto preferences = runtime.preferences().overlays;
    if (!preferences.chunkBorders && !preferences.hitboxes) return;
    IClientInstance& client = context.mClientInstance;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    try {
        auto& dimension = player->getDimension();
        if (preferences.chunkBorders) {
            auto const& range = dimension.mHeightRange;
            Vec3 const position = player->getPosition();
            thread_local ChunkBorderCache borders;
            drawLines(context, borders.get({position.x,position.y,position.z}, range->mMin, range->mMax));
        }
        if (preferences.hitboxes && context.mImpl) {
            Vec3 const camera = context.mImpl->mCameraPosition;
            std::vector<Line> lines;
            // Only borrow client actors during this pass. No entity pointers or
            // bounds survive world exit or a subsequent frame.
            for (auto* actor : player->getLevel().getRuntimeActorList()) {
                if (!actor || actor == player || &actor->getDimension() != &dimension) continue;
                auto const& bounds = actor->getAABB();
                Point min{bounds.min.x,bounds.min.y,bounds.min.z}, max{bounds.max.x,bounds.max.y,bounds.max.z};
                if (!hitboxInRange(min,max,{camera.x,camera.y,camera.z},preferences.hitboxDistance)) continue;
                auto edges = wireBox(min,max);
                lines.insert(lines.end(),edges.begin(),edges.end());
            }
            drawLines(context,lines,true);
        }
    } catch (std::exception const& error) {
        // Rate-limit repeated failures without swallowing the vanilla pass.
        static bool reported = false;
        if (!reported) { runtime.self().getLogger().error("World overlay drawing failed: {}", error.what()); reported = true; }
    }
}
}
void start() {
    if (installed) return;
    installed = WorldLines::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install world overlay render hook");
}
void stop() {
    if (installed && WorldLines::unhook(true)) installed = false;
}
}
