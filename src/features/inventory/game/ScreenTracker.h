#pragma once

#include "ll/api/event/ListenerBase.h"

#include <memory>

class ContainerScreenController;
class ScreenController;
class ScreenView;

namespace ll::event::inline render {
class AfterUIRenderEvent;
}

namespace lamium::inventory::game {

/// Remembers which container screen (survival inventory, chest, ...) is
/// currently shown, so an explicit Sort trigger can act on it.
///
/// The controller is observed from the game's own UI render callback and held
/// through a weak pointer to the ScreenView's shared controller, so a screen
/// that has been torn down can never be dereferenced. Leaving the screen also
/// drops it eagerly via the `onLeave` hook.
///
/// All members run on the client's main (render/input) thread.
class ScreenTracker {
public:
    static ScreenTracker& getInstance();

    void install();
    void uninstall();

    /// The active container screen controller, or null when no container
    /// screen is open (or it has already been destroyed).
    [[nodiscard]] std::shared_ptr<ContainerScreenController> current() const;

    /// The ScreenView that owns `current()`; only meaningful while
    /// `current()` is non-null (the view outlives its controller's onLeave).
    [[nodiscard]] ScreenView const* currentView() const { return mCurrentView; }

    /// Called from the onLeave hook.
    void onControllerLeft(ContainerScreenController& controller);

private:
    void onAfterUIRender(ll::event::AfterUIRenderEvent& event);

    std::weak_ptr<ScreenController> mCurrent;
    ScreenView const*               mCurrentView{nullptr};
    ll::event::ListenerPtr          mRenderListener;
    ll::event::ListenerPtr          mExitListener;
    bool                            mInstalled{false};
};

} // namespace lamium::inventory::game
