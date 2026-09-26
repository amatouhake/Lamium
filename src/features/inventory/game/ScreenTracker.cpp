#include "features/inventory/game/ScreenTracker.h"

#include "features/inventory/game/TextInputTracker.h"
#include "features/inventory/game/SortSession.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "app/Runtime.h"
#include <stdexcept>
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/gui/screens/ScreenController.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/controllers/ContainerScreenController.h"
#ifdef LAMIUM_RESTOCK_TRACE
#include "features/inventory/game/RestockTrace.h"
#endif

namespace lamium::inventory::game {

namespace {

// Closing a screen does not always render a final frame we could observe, so
// forget the controller as soon as the game tells it to leave. The base
// implementation is reached by every container screen subclass.
LL_TYPE_INSTANCE_HOOK(
    ContainerScreenLeaveHook,
    ll::memory::HookPriority::Normal,
    ContainerScreenController,
    &ContainerScreenController::$onLeave,
    void
) {
    ScreenTracker::getInstance().onControllerLeft(*this);
    origin();
}

} // namespace

ScreenTracker& ScreenTracker::getInstance() {
    static ScreenTracker instance;
    return instance;
}

void ScreenTracker::install() {
    if (mInstalled) return;
    if (ContainerScreenLeaveHook::hook(true) != 0) throw std::runtime_error("Could not install inventory screen hook");
    mInstalled = true;
    mRenderListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>(
        [this](ll::event::AfterUIRenderEvent& event) { onAfterUIRender(event); }
    );
    mExitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>(
        [this](auto&) {
            SortSession::cancel("world exited");
            TextInputTracker::getInstance().forget(mCurrentView);
            mCurrent.reset();
            mCurrentView = nullptr;
        }
    );
}

void ScreenTracker::uninstall() {
    SortSession::cancel("screen tracking stopped");
    if (!mInstalled) return;
    if (mRenderListener) {
        ll::event::EventBus::getInstance().removeListener(mRenderListener);
        mRenderListener.reset();
    }
    if (mExitListener) {
        ll::event::EventBus::getInstance().removeListener(mExitListener);
        mExitListener.reset();
    }
    if (ContainerScreenLeaveHook::unhook(true)) mInstalled = false;
    else Runtime::instance().self().getLogger().error("Could not remove inventory screen hook");
    mCurrent.reset();
    mCurrentView = nullptr;
}

std::shared_ptr<ContainerScreenController> ScreenTracker::current() const {
    auto controller = mCurrent.lock();
    if (!controller) return nullptr;
    // Only controllers that reported _isContainerScreen() are ever stored.
    return std::static_pointer_cast<ContainerScreenController>(controller);
}

void ScreenTracker::onControllerLeft(ContainerScreenController& controller) {
    auto current = mCurrent.lock();
    if (current && current.get() == static_cast<ScreenController*>(&controller)) {
        SortSession::cancel("container screen closed");
        mCurrent.reset();
        TextInputTracker::getInstance().forget(mCurrentView);
        mCurrentView = nullptr;
    }
}

void ScreenTracker::onAfterUIRender(ll::event::AfterUIRenderEvent& event) {
    // Every ScreenView on the stack renders each frame; keep the most recent
    // container screen. The HUD and other overlays are not container screens.
    auto const& controller = event.screenView().mController;
    if (!controller || !controller->_isContainerScreen()) {
        return;
    }
    if (!event.screenView().mHasFocus) {
        if (mCurrent.lock() == controller) {
            SortSession::cancel("container screen lost focus");
            mCurrent.reset();
            mCurrentView = nullptr;
        }
        return;
    }
    if (mCurrent.lock() != controller) {
        SortSession::cancel("container screen changed");
        mCurrent     = controller;
        mCurrentView = &event.screenView();
#ifdef LAMIUM_RESTOCK_TRACE
        // L-17: record the working screen path's transfer context for
        // comparison with the HUD controller. Read-only.
        try {
            auto screen = std::static_pointer_cast<ContainerScreenController>(controller);
            if (auto manager = screen->mContainerManagerController) restockTrace::inspectScreenController(*manager);
        } catch (...) {}
#endif
        // Do not erase text focus here: a search box may already have gained
        // focus before the first rendered frame. onLeave handles old views.
    }
    try { SortSession::tick(*std::static_pointer_cast<ContainerScreenController>(controller)); }
    catch (std::exception const& error) {
        SortSession::cancel();
        Runtime::instance().self().getLogger().error("Sort stopped: {}", error.what());
    }
}

} // namespace lamium::inventory::game
