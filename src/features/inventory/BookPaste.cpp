#include "features/inventory/BookPaste.h"
#include "app/Capabilities.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ui/Localization.h"
#include "ui/Toast.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/TextEditComponent.h"
#include "mc/client/gui/screens/controllers/BookScreenController.h"
#include "mc/client/gui/screens/managers/BookScreenManager.h"
#include "mc/deps/input/TextEditContext.h"
#include "mc/world/item/PageContent.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lamium::inventory::bookPaste {
namespace {
using Clock = std::chrono::steady_clock;
// A book screen's controller and page boxes live as long as the screen. The
// controller is remembered from the screen's own frame call and the boxes from
// their own focus and text calls, so no game pointer outlives its screen:
// the screen keeps refreshing the stamp, and losing it drops both. The
// generation tells two screens apart that a heap reuse put at one address.
struct Screen {
    BookScreenController* controller = nullptr;
    TextEditComponent* boxes[2]{}; // the spread's two page boxes, as they are seen
    Clock::time_point seen{};
    unsigned long long generation = 0;
    unsigned long long frame = 0;
};
// What the paste left for the book's next frame: the pages of its overflow and
// a refusal to show. Owned copies only; the screen it belongs to comes from the
// page plan's identity.
struct Pending {
    DeferredPages pages;
    unsigned long long frame = 0; // the frame the paste arrived on
    int needed = 0, free = 0;     // refusal message
    bool refused = false;
    bool shown = false;
};
std::mutex stateMutex;
Screen screen;
std::optional<Pending> pending;
// A generation that only ever grows, so a screen that reuses a closed
// controller's address is not mistaken for that controller.
unsigned long long generations = 0;
// One book frame or two; a paste after a stall into a forgotten screen must fall
// through to the game instead of writing into a stale controller.
constexpr auto screenAlive = std::chrono::milliseconds(250);
// Our own text writes must not be taken for a new paste.
thread_local bool insidePaste = false;

// The page box is the only book text box with the page's character limit and
// the page rectangle's constraint; the title and author take 512 and do not
// constrain.
bool pageBox(TextEditComponent const& box) noexcept {
    return box.mMaxLength == static_cast<uint>(pageLimit) && box.mConstrainToRect;
}
void rememberController(BookScreenController& controller) noexcept {
    std::lock_guard lock(stateMutex);
    auto const now = Clock::now();
    // Reopening a book makes new page boxes (seen in game 2026-11-06), and the new
    // controller can reuse the old address, so a gap in the frames also ends a
    // screen. A long stall only costs the boxes until their next focus call.
    if (screen.controller != &controller || now - screen.seen > screenAlive) {
        screen.controller = &controller;
        screen.boxes[0] = screen.boxes[1] = nullptr; // another screen's boxes
        screen.generation = ++generations;
        pending.reset();           // a paste belongs to the screen that made it
    }
    ++screen.frame;
    screen.seen = now;
}
void rememberBox(TextEditComponent& box) noexcept {
    std::lock_guard lock(stateMutex);
    if (!screen.controller || Clock::now() - screen.seen > screenAlive) return;
    if (screen.boxes[0] == &box || screen.boxes[1] == &box) return;
    screen.boxes[1] = screen.boxes[0];
    screen.boxes[0] = &box;
}
struct Current {
    BookScreenController* controller = nullptr;
    TextEditComponent* box = nullptr;
    unsigned long long generation = 0;
    unsigned long long frame = 0;
};
std::optional<Current> current() {
    std::lock_guard lock(stateMutex);
    if (!screen.controller || Clock::now() - screen.seen > screenAlive) return std::nullopt;
    // The box being edited is the selected one. Focus follows the mouse over
    // the other half (seen in game 2026-11-06), so it says nothing here.
    TextEditComponent* selected = nullptr;
    for (auto* box : screen.boxes) {
        if (box && box->mSelected) {
            selected = box;
            break;
        }
    }
    return Current{screen.controller, selected, screen.generation, screen.frame};
}
void showRefusal(int needed, int free) noexcept {
    try { ui::showWarningToast(ui::translated("bookPaste.tooManyPages", needed, free)); } catch (...) {}
}
void forget(BookScreenController& controller) noexcept {
    std::optional<Pending> left;
    {
        std::lock_guard lock(stateMutex);
        if (screen.controller != &controller) return;
        screen = {};
        left = pending;
        pending.reset();
    }
    if (left && left->refused) showRefusal(left->needed, left->free);
}

// What the paste detour must do: leave the paste to the game, insert the part
// that fits and let the next book frame add the rest, or paste nothing and say
// how many pages it needed.
struct Outcome {
    enum class Kind { Vanilla, Insert, Refused } kind = Kind::Vanilla;
    std::string insert; // the shortened text the game's own insert gets
    std::string head;   // the text the first page must hold afterwards
    bool trim = false;  // the text after the caret moved, so the box must hold head
    int caret = 0;      // where the caret stands on the first page afterwards
};

// Decides a paste and keeps the overflow for the next book frame. The page model
// is never written here: this runs inside the text edit's own call, and adding
// pages rebuilds the page UI, which invalidates the context being edited (the
// L-259 crash at origin()).
Outcome handlePaste(TextEditContext& context, std::string_view paste) {
    auto at = current();
    if (paste.empty()) return {};
    auto settings = Runtime::instance().snapshot();
    if (!settings->ui.bookPaste) return {};
    if (!versionSensitiveAllowed(capabilities::Id::BookPastePages)) return {};
    if (!pageContextLimit(context.mMaxLength)) return {}; // not the page box L-259 knows
    if (!at) return {};
    auto& controller = *at->controller;
    if (!at->box) return {};
    if (!controller.mEditable) return {};
    {
        // A second paste before the next book frame would replace the first
        // one's kept pages, and its first part would already be on the page:
        // paste nothing until those pages are in.
        std::lock_guard lock(stateMutex);
        if (pending && !pending->pages.extra.empty()) return Outcome{Outcome::Kind::Refused};
    }
    if (controller.mCurrentView != BookScreenController::BookView::Viewing) return {};
    auto& pages = controller.mPages.get();
    auto const& cursor = controller.mPageHasCursor.get();
    // The green caret names the page, and the box's own text is what its position
    // counts. Nothing here reads the page model's text: it lags the box until the
    // player leaves the page. A photo page holds no text to edit.
    int const index = caretPage(controller.mCurrentPage, cursor[0], cursor[1], static_cast<int>(pages.size()));
    if (index < 0) return {};
    PageContent& page = pages[static_cast<std::size_t>(index)];
    if (!page.mPhotoName.get().empty()) return {};
    std::string_view const text = context.mCurrentUtf8Text.get();
    int const chars = codePointCount(text);
    auto const& selection = context.mSelection.get();
    // The game counts characters; a caret past the end means the units or the
    // text changed under us, so the game keeps the paste.
    if (context.mCaret < 0 || context.mCaret > chars || selection.mStartCaretPos > chars
        || selection.mEndCaretPos > chars)
        return {};
    int caret = context.mCaret;
    int start = caret, end = caret;
    int const from = selection.mStartCaretPos;
    int const to = selection.mEndCaretPos;
    if (from != to) {
        start = std::min(from, to);
        end = std::max(from, to);
    }
    std::string const before(text.substr(0, byteOffset(text, start)));
    std::string const after(text.substr(byteOffset(text, end)));
    int const free = maxPages - static_cast<int>(pages.size());
    // The game's own test: the page box's character limit and its own text-fit
    // test, so wrapping and fonts need no copy here.
    auto fits = [&](std::string const& candidate) {
        return codePointCount(candidate) <= pageLimit && at->box->_textFitsInControl(candidate);
    };
    auto split = splitPaste(before, after, paste, free, pageLimit, fits);
    if (split.kind == SplitKind::Vanilla) return {};
    if (split.kind == SplitKind::Refused) {
        {
            std::lock_guard lock(stateMutex);
            Pending item;
            item.frame = at->frame;
            item.needed = split.needed;
            item.free = split.free;
            item.refused = true;
            pending = std::move(item);
        }
        // Shown from the next book frame, never from the paste's own call.
        return Outcome{Outcome::Kind::Refused};
    }
    // The page keeps the text before the caret plus the part of the paste that
    // fits; everything else becomes pages the next book frame inserts. Only
    // owned text survives this call.
    Outcome outcome;
    outcome.kind = Outcome::Kind::Insert;
    outcome.head = split.pages[0];
    if (!outcome.head.starts_with(before)) return {}; // the split must keep the page's head
    outcome.insert = outcome.head.substr(before.size());
    outcome.trim = !after.empty();
    outcome.caret = codePointCount(outcome.head);
    DeferredPages plan;
    plan.screen = ScreenId{reinterpret_cast<std::uintptr_t>(at->controller), at->generation};
    plan.frame = at->frame;
    plan.index = index;
    plan.head = outcome.head;
    plan.extra.assign(std::make_move_iterator(split.pages.begin() + 1), std::make_move_iterator(split.pages.end()));
    {
        std::lock_guard lock(stateMutex);
        Pending item;
        item.pages = std::move(plan);
        item.frame = at->frame;
        pending = std::move(item);
    }
    return outcome;
}

// Applies the pages a paste kept, from the book screen's own frame. This is the
// only place the page model is written: a page add rebuilds the page UI, so
// doing it inside the text edit's call freed the context the paste was running
// in (the L-259 crash).
void applyPages(BookScreenController& controller) noexcept {
    std::optional<DeferredPages> plan;
    ScreenId id;
    {
        std::lock_guard lock(stateMutex);
        if (!pending || (pending->pages.extra.empty() && pending->pages.head.empty())) return;
        if (!planDue(pending->pages.frame, screen.frame)) return; // queued this frame
        plan = pending->pages;
        pending->pages.extra.clear(); // one attempt only
        pending->pages.head.clear();
        id = ScreenId{reinterpret_cast<std::uintptr_t>(&controller), screen.generation};
    }
    auto& pages = controller.mPages.get();
    int const count = static_cast<int>(pages.size());
    // The box's new text does not reach the page model by itself after the box was
    // set directly, and leaving the page then sent the old text to the server and
    // loaded it back (seen in game 2026-11-06), so the model is told here.
    if (id == plan->screen && plan->index >= 0 && plan->index < count) {
        try {
            pages[static_cast<std::size_t>(plan->index)].mText.get() = plan->head;
            controller._parsePageText(plan->index);
        } catch (...) {}
    }
    if (plan->extra.empty()) return;
    ApplyPlan const decision = planApply(*plan, id, static_cast<bool>(controller.mEditable), count);
    if (!decision.apply()) {
        // The first page is already in; the rest stays on the clipboard only.
        if (decision.skip != ApplySkip::Screen)
            try { ui::showWarningToast(ui::translated("bookPaste.restDropped")); } catch (...) {}
        return;
    }
    try {
        for (std::size_t k = 0; k < plan->extra.size(); ++k) {
            controller._addPageAt(decision.index + static_cast<int>(k),
                                  PageContent(std::string(plan->extra[k]), std::nullopt));
        }
        // The pages hold their text, but the screen shows a page from its
        // parsed form and only redraws when it is told to: without the parse
        // and the refresh the added pages stay blank until the player puts the
        // caret on them or reopens the book (seen in game 2026-10-27).
        for (std::size_t k = 0; k < plan->extra.size(); ++k)
            controller._parsePageText(decision.index + static_cast<int>(k));
        // The book stays on the pasted page. Writing mCurrentPage turns the shown
        // spread but leaves the box holding this page's text, which the game then
        // saved over the target page (seen in game 2026-11-06). A real turn needs
        // the game's own call, which does not link.
        controller.mForceUIRefresh = true;
        controller.mShouldRefreshText = true;
        // The box stays remembered: the page boxes outlive added pages (the same
        // two addresses across every add, seen in game 2026-11-06), and no focus
        // call follows when the shown page does not change, so forgetting it here
        // sent the next paste to the game.
    } catch (...) {}
}
// The refusal's toast waits for the book's next frame too: the HUD is not drawn
// while the book is open, so it is shown again when the screen goes away.
void showPendingRefusal() noexcept {
    int needed = 0, free = 0;
    {
        std::lock_guard lock(stateMutex);
        if (!pending || !pending->refused || pending->shown) return;
        if (!planDue(pending->frame, screen.frame)) return;
        pending->shown = true;
        needed = pending->needed;
        free = pending->free;
    }
    showRefusal(needed, free);
}
void cancelPending() noexcept {
    std::lock_guard lock(stateMutex);
    pending.reset();
}

LL_TYPE_INSTANCE_HOOK(BookScreenTick, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::$preFrameTick, void) {
    origin();
    try {
        rememberController(*this);
        applyPages(*this);
        showPendingRefusal();
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(BookScreenValid, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::$_isStillValid, bool) {
    bool valid = origin();
    try { if (!valid) forget(*this); } catch (...) {}
    return valid;
}
LL_TYPE_INSTANCE_HOOK(PageBoxFocus, ll::memory::HookPriority::Normal, TextEditComponent,
    &TextEditComponent::handleFocusSet, void, bool focused) {
    if (focused) {
        try { if (pageBox(*this)) rememberBox(*this); } catch (...) {}
    }
    origin(focused);
}
LL_TYPE_INSTANCE_HOOK(TextTrySet, ll::memory::HookPriority::Normal, TextEditComponent,
    &TextEditComponent::trySetText, bool, std::string text) {
    bool accepted = origin(std::move(text));
    try { if (accepted && pageBox(*this)) rememberBox(*this); } catch (...) {}
    return accepted;
}
LL_TYPE_INSTANCE_HOOK(ContextText, ll::memory::HookPriority::Normal, TextEditContext,
    &TextEditContext::feedText, void, std::string_view utf8Text) {
    Outcome outcome;
    if (!insidePaste) {
        try { outcome = handlePaste(*this, utf8Text); } catch (...) { outcome = Outcome{}; }
    }
    // The paste's own insert runs the game's call with the shortened text; the
    // page model is left to the book screen's next frame (applyPages). A paste
    // that fits, a gate that fails and a refusal keep the game's own path.
    insidePaste = true;
    try {
        switch (outcome.kind) {
        case Outcome::Kind::Vanilla: origin(utf8Text); break;
        case Outcome::Kind::Insert:
            if (!outcome.insert.empty()) origin(outcome.insert);
            // The text after the caret belongs to the last new page, so the page
            // and its box keep only the first page's text. The box is what the
            // screen writes into the page model, so setting the context alone
            // left the tail at the end of the pasted page (seen in game
            // 2026-10-27).
            if (outcome.trim) {
                _setText(outcome.head);
                if (auto at = current(); at && at->box) at->box->trySetText(outcome.head);
            }
            // The calls above leave the caret wherever they left it. Without this
            // it wandered (66, 103, 140 for one spot, seen in game 2026-11-06).
            mCaret = outcome.caret;
            // Equal ends mean no selection, whatever their value: a selection the
            // paste replaced must not stay marked. The caret alone says where.
            mSelection.get().mStartCaretPos = 0;
            mSelection.get().mEndCaretPos = 0;
            break;
        case Outcome::Kind::Refused: break;
        }
    } catch (...) {
        cancelPending();
    }
    insidePaste = false;
}
struct Hook {
    char const* name;
    int (*install)(bool);
    bool (*remove)(bool);
    bool installed = false;
};
Hook hooks[] = {
    {"BookScreenTick", BookScreenTick::hook, BookScreenTick::unhook},
    {"BookScreenValid", BookScreenValid::hook, BookScreenValid::unhook},
    {"PageBoxFocus", PageBoxFocus::hook, PageBoxFocus::unhook},
    {"TextTrySet", TextTrySet::hook, TextTrySet::unhook},
    {"ContextText", ContextText::hook, ContextText::unhook},
};
}
bool start() {
    // A hook that does not attach only narrows the feature: the paste then takes
    // the game's own path, so the feature still starts and says what is missing.
    for (auto& hook : hooks) {
        if (hook.installed) continue;
        try {
            if (hook.install(true) != 0) {
                try {
                    Runtime::instance().self().getLogger().warn("L-259 book paste: {} did not attach", hook.name);
                } catch (...) {}
                continue;
            }
            hook.installed = true;
        } catch (std::exception const& error) {
            try {
                Runtime::instance().self().getLogger().warn("L-259 book paste: {} did not attach: {}", hook.name,
                                                            error.what());
            } catch (...) {}
        } catch (...) {}
    }
    return true;
}
void stop() {
    {
        std::lock_guard lock(stateMutex);
        screen = {};
        pending.reset();
    }
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) {
        if (!it->installed) continue;
        try {
            if (it->remove(true)) it->installed = false;
        } catch (...) {}
    }
}
}
