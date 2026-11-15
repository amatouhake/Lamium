#include "features/research/WorldProbe.h"
#ifdef LAMIUM_WORLD_PROBE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#include "mc/world/containers/managers/controllers/AnvilContainerManagerController.h"
#include "mc/world/containers/managers/controllers/ContainerManagerController.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/deps/ecs/gamerefs_entity/EntityContext.h"
#include "mc/entity/components/AgeableComponent.h"
#include "mc/entity/components_json_legacy/BreedableComponent.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/monster/EnderDragon.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantResult.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/world/level/Level.h"
#include "mc/world/phys/AABB.h"
#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>

// What the client can read, every 5 s and bounded; nothing is written:
// L-249 the client level's chunk tick range, L-33 the age and breeding
// components of nearby actors, L-30 the Ender Dragon's part boxes.
namespace lamium::researchTrace::worldProbe {
namespace {
using Clock = std::chrono::steady_clock;
template <class... Args>
void log(std::format_string<Args...> format, Args&&... args) noexcept {
    try { Runtime::instance().self().getLogger().info("{}", std::format(format, std::forward<Args>(args)...)); }
    catch (...) {}
}
Clock::time_point last = Clock::now() - std::chrono::hours(1);
unsigned lines = 0;
bool enchantsListed = false;
// L-170, once: the enchantment registry, and what the held item accepts.
void listEnchants(LocalPlayer& player) {
    ItemStack const& held = player.getSelectedItem();
    log("probe L-170 held={} enchantSlot={:#x}", held.getTypeName(), held.mItem ? held.mItem->getEnchantSlot() : 0);
    for (auto const& enchant : Enchant::mEnchants()) {
        if (!enchant) continue;
        Enchant::Type const type = enchant->mEnchantType;
        int const most = enchant->getMaxLevel();
        EnchantmentInstance wanted;
        wanted.mEnchantType = type;
        wanted.mLevel = most;
        auto const answer = EnchantUtils::canEnchant(held, wanted, false);
        std::string conflicts;
        for (auto const& other : Enchant::mEnchants())
            if (other && other.get() != enchant.get() && !enchant->isCompatibleWith(other->mEnchantType))
                conflicts += std::format("{} ", static_cast<int>(static_cast<Enchant::Type>(other->mEnchantType)));
        ++lines;
        log("probe L-170 enchant={} id={} frequency={} maxLevel={} primary={:#x} secondary={:#x} compatibility={} "
            "disabled={} held={} conflicts={}",
            static_cast<int>(type), static_cast<HashedString const&>(enchant->mStringId).getString(),
            static_cast<int>(static_cast<Enchant::Frequency>(enchant->mFrequency)), most,
            static_cast<int>(enchant->mPrimarySlots), static_cast<int>(enchant->mSecondarySlots),
            static_cast<int>(enchant->mCompatibility), static_cast<bool>(enchant->mIsDisabled),
            static_cast<int>(static_cast<EnchantResultType>(answer.result)), conflicts);
    }
}
void probe(LocalPlayer& player) {
    if (Clock::now() - last < std::chrono::seconds(5) || lines >= 4000) return;
    last = Clock::now();
    if (!enchantsListed) { enchantsListed = true; listEnchants(player); }
    auto& level = player.getLevel();
    ++lines;
    log("probe L-249 chunkTickRange={} dimension={}", level.getChunkTickRange(), static_cast<int>(player.getDimensionId()));
    Vec3 const here = player.getPosition();
    int listed = 0;
    for (auto* actor : level.getRuntimeActorList()) {
        if (!actor || actor == &player) continue;
        Vec3 const at = actor->getPosition();
        float const dx = at.x - here.x, dy = at.y - here.y, dz = at.z - here.z;
        bool const dragon = actor->getTypeName() == "minecraft:ender_dragon";
        // Fish and dropped items crowd out the actors worth reading.
        auto age = actor->getEntityContext().tryGetComponent<AgeableComponent>();
        auto breed = actor->getEntityContext().tryGetComponent<BreedableComponent>();
        bool const plain = !dragon && !actor->isBaby() && !age && !breed;
        if (!dragon && (dx * dx + dy * dy + dz * dz > 24 * 24 || (plain && listed >= 4))) continue;
        if (plain) ++listed;
        ++lines;
        log("probe L-33 actor={} baby={} ageable={} age={} breedable={} loveTimer={} breedCooldown={}",
            actor->getTypeName(), actor->isBaby(), static_cast<bool>(age), age ? static_cast<int>(age->mAge) : 0,
            static_cast<bool>(breed), breed ? static_cast<int>(breed->mLoveTimer) : 0,
            breed ? static_cast<int>(breed->mBreedCooldown) : 0);
        if (!dragon) continue;
        auto& parts = static_cast<EnderDragon&>(*actor);
        std::pair<char const*, AABB*> const boxes[]{{"head", parts.mHead}, {"neck", parts.mNeck}, {"body", parts.mBody},
            {"tail1", parts.mTail1}, {"tail2", parts.mTail2}, {"tail3", parts.mTail3}, {"wing1", parts.mWing1},
            {"wing2", parts.mWing2}};
        auto const& whole = actor->getAABB();
        ++lines;
        log("probe L-30 dragon pos={:.1f},{:.1f},{:.1f} aabb={:.1f},{:.1f},{:.1f}..{:.1f},{:.1f},{:.1f}", at.x, at.y, at.z,
            whole.min.x, whole.min.y, whole.min.z, whole.max.x, whole.max.y, whole.max.z);
        for (auto const& [name, box] : boxes) {
            ++lines;
            if (!box) { log("probe L-30 part={} null", name); continue; }
            log("probe L-30 part={} {:.1f},{:.1f},{:.1f}..{:.1f},{:.1f},{:.1f}", name,
                box->min.x, box->min.y, box->min.z, box->max.x, box->max.y, box->max.z);
        }
    }
}
LL_TYPE_INSTANCE_HOOK(ProbeHook, ll::memory::HookPriority::Low, LevelRendererPlayer,
    &LevelRendererPlayer::getFov, float, float alpha, bool variable) {
    float result = origin(alpha, variable);
    try {
        if (auto* player = static_cast<IClientInstance&>(mClientInstance).getLocalPlayer()) probe(*player);
    } catch (...) {}
    return result;
}
// L-170: the anvil screen's name, its controls' boxes (once per opening),
// and once a second the game's own cost and what the slots hold.
ll::event::ListenerPtr screenListener;
Clock::time_point lastScreen = Clock::now() - std::chrono::hours(1);
void const* treeOf = nullptr;
unsigned screenLines = 0;
int ticksHere = 0;
float viewWidth = 0;
// Only the larger boxes narrower than the screen: the window and its parts.
void boxes(UIControl const& control, int depth) {
    if (depth > 16 || screenLines >= 400) return;
    if ((static_cast<int>(control.mVisible) & 1) == 0) return;
    glm::vec2 const at = *control.mCachedPosition, size = *control.mSize;
    if (size.x >= 60 && size.y >= 12 && size.x < viewWidth - 1) {
        ++screenLines;
        log("probe L-170 control depth={} name={} at={:.0f},{:.0f} size={:.0f}x{:.0f}", depth, *control.mName, at.x, at.y,
            size.x, size.y);
    }
    for (auto const& child : *control.mChildren)
        if (child) boxes(*child, depth + 1);
}
void anvilScreen(ll::event::AfterUIRenderEvent& event) {
    auto& view = event.screenView();
    if (!view.mHasFocus || !view.mController || !view.mController->_isContainerScreen() || screenLines >= 400) return;
    std::string const name = view.getScreenName();
    if (name.find("anvil") == std::string::npos) { treeOf = nullptr; return; }
    auto& screen = static_cast<ContainerScreenController&>(*view.mController);
    auto manager = screen.mContainerManagerController;
    if (!manager) return;
    if (treeOf != &view) {
        treeOf = &view;
        ticksHere = 0;
        glm::vec2 const size = *view.mSize;
        viewWidth = size.x;
        ++screenLines;
        log("probe L-170 screen={} view={:.0f}x{:.0f} containerType={}", name, size.x, size.y,
            static_cast<int>(manager->getContainerType()));
    }
    if (Clock::now() - lastScreen < std::chrono::seconds(1)) return;
    lastScreen = Clock::now();
    // The first frames are not laid out yet.
    if (++ticksHere == 2)
        if (auto* tree = view.mVisualTree.get(); tree && tree->mRootControl) boxes(*tree->mRootControl.get(), 0);
    std::string held;
    size_t visited = 0;
    for (auto const& [collection, container] : manager->mContainers.get()) {
        if (++visited > 32) break;
        if (!container) continue;
        int const size = manager->getContainerSize(collection);
        if (size > 4) { held += std::format(" {}[{}]", collection, size); continue; }
        for (int i = 0; i < size; ++i) {
            auto const& item = manager->getItemStack(collection, i);
            held += item.isNull() ? std::format(" {}[{}]=empty", collection, i)
                                  : std::format(" {}[{}]={} repairCost={}", collection, i, item.getTypeName(),
                                        item.getBaseRepairCost());
        }
    }
    ++screenLines;
    log("probe L-170 anvil cost={}{}", static_cast<int>(static_cast<AnvilContainerManagerController&>(*manager).mCost), held);
}
bool installed = false;
}
void start() {
    installed = ProbeHook::hook(true) == 0;
    if (!installed) throw std::runtime_error("Could not install the world probe");
    screenListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(
        [](auto& event) { try { anvilScreen(event); } catch (...) {} });
    Runtime::instance().self().getLogger().warn("World probe enabled (L-249, L-33, L-30; read only)");
}
void stop() {
    if (screenListener) { ll::event::EventBus::getInstance().removeListener(screenListener); screenListener.reset(); }
    if (installed && ProbeHook::unhook(true)) installed = false;
}
}
#else
namespace lamium::researchTrace::worldProbe { void start() {} void stop() {} }
#endif
