#include "features/inventory/game/TextInputTracker.h"

#include "app/Runtime.h"

#include "ll/api/memory/Hook.h"
#include <stdexcept>

#include "mc/client/gui/screens/ScreenView.h"

namespace lamium::inventory::game {

namespace {

// The game raises this for every text box when it gains (true) or loses
// (false) its selected/editing state.
LL_TYPE_INSTANCE_HOOK(
    TextEditSelectedHook,
    ll::memory::HookPriority::Normal,
    ScreenView,
    &ScreenView::_fireSelectedStateChangeEvent,
    void,
    ::TextEditComponent const& textEditComponent,
    bool                       state
) {
    TextInputTracker::getInstance().onSelectedStateChanged(*this, textEditComponent, state);
    origin(textEditComponent, state);
}

} // namespace

TextInputTracker& TextInputTracker::getInstance() {
    static TextInputTracker instance;
    return instance;
}

void TextInputTracker::install() {
    if (mInstalled) return;
    if (TextEditSelectedHook::hook(true) != 0) throw std::runtime_error("Could not install inventory text focus hook");
    mInstalled = true;
}

void TextInputTracker::uninstall() {
    if (!mInstalled) return;
    if (TextEditSelectedHook::unhook(true)) mInstalled = false;
    else Runtime::instance().self().getLogger().error("Could not remove inventory text focus hook");
    mSelected.clear();
}

bool TextInputTracker::isEditing(ScreenView const* view) const {
    auto it = mSelected.find(view);
    return it != mSelected.end() && !it->second.empty();
}

void TextInputTracker::onSelectedStateChanged(
    ScreenView const&        view,
    TextEditComponent const& component,
    bool                     selected
) {
    auto& set = mSelected[&view];
    if (selected) {
        set.insert(&component);
    } else {
        set.erase(&component);
        if (set.empty()) mSelected.erase(&view);
    }
    Runtime::instance().self().getLogger().debug(
        "Text box {} on screen {:p} ({} selected)",
        selected ? "selected" : "deselected",
        static_cast<void const*>(&view),
        isEditing(&view) ? "still" : "none"
    );
}

void TextInputTracker::forget(ScreenView const* view) { mSelected.erase(view); }

} // namespace lamium::inventory::game
