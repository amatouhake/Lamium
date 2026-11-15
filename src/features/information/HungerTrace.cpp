// L-63 research: does the client receive saturation, and where does the
// vanilla hunger bar sit? Logs on change only, and outlines where the
// drumsticks are assumed to be so a screenshot shows the offset.
#ifdef LAMIUM_HUNGER_TRACE
#include "features/information/HungerTrace.h"
#include "app/Runtime.h"
#include "ui/Widgets.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/components/IFoodItemComponent.h"
#include <format>
#include <optional>
#include <string>

namespace lamium::information {
namespace {
void logChanged(std::string& last, std::string line) {
    if (line == last) return;
    last = line;
    Runtime::instance().self().getLogger().info("Hunger trace: {}", line);
}
std::optional<float> attribute(Player& player, Attribute const& which) {
    auto ref = player.getAttribute(which);
    AttributeInstance const* instance = ref.mPtr;
    if (!instance) return std::nullopt;
    return instance->mCurrentValue;
}
std::string value(std::optional<float> v) { return v ? std::format("{:.2f}", *v) : std::string("none"); }
}
void traceHunger(MinecraftUIRenderContext& context, ScreenView const& view) noexcept {
    try {
        auto* player = context.mClient.getLocalPlayer();
        if (!player) return;
        static std::string lastValues, lastControl, lastFood;
        auto hunger = attribute(*player, Player::HUNGER());
        auto saturation = attribute(*player, Player::SATURATION());
        // Exhaustion changes every step; log it coarsely.
        auto exhaustion = attribute(*player, Player::EXHAUSTION());
        logChanged(lastValues, std::format("hunger {} saturation {} exhaustion {:.0f}", value(hunger), value(saturation),
                                           exhaustion ? *exhaustion : -1.f));
        ItemStack const& held = player->getSelectedItem();
        std::string food = "held none";
        if (!held.isNull() && held.mItem) {
            if (auto* component = held.mItem->getFood())
                food = std::format("held {} nutrition {} saturation modifier {:.2f}", held.getTypeName(),
                                   component->getNutrition(), component->getSaturationModifier());
            else food = std::format("held {} not food", held.getTypeName());
        }
        logChanged(lastFood, food);
        VisualTree* tree = view.mVisualTree.get();
        if (!tree) return;
        auto control = tree->getControlByName("hunger_rend", true);
        glm::vec2 screen = *view.mSize;
        if (!control) { logChanged(lastControl, std::format("no hunger_rend on {:.0f}x{:.0f}", screen.x, screen.y)); return; }
        glm::vec2 p = *control->mCachedPosition, s = *control->mSize;
        logChanged(lastControl, std::format("hunger_rend at {:.1f},{:.1f} size {:.1f}x{:.1f} dirty {} on {:.1f}x{:.1f}", p.x,
                                            p.y, s.x, s.y, static_cast<bool>(control->mCachedPositionDirty), screen.x,
                                            screen.y));
        // Assumed layout: ten 9x9 icons right to left from the control, 8 apart.
        for (int i = 0; i < 10; ++i)
            ui::frame(context, p.x - 8 - 8 * i, p.y, 9, 9, ui::palette::warning, 1.f);
    } catch (...) {
    }
}
}
#endif
