#include "features/inventory/BookPaste.h"
#include "app/Runtime.h"
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
#ifdef LAMIUM_BOOK_PASTE_TRACE
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/BookEditPacket.h"
#include <Windows.h>
#include <atomic>
#include <format>
#include <variant>
#endif

#ifdef LAMIUM_BOOK_PASTE_TRACE
#define BOOK_PASTE_TRACE(...) do { try { log(std::format(__VA_ARGS__)); } catch (...) {} } while (false)
#else
#define BOOK_PASTE_TRACE(...) do {} while (false)
#endif

namespace lamium::inventory::bookPaste {
namespace {
using Clock = std::chrono::steady_clock;
// A book screen's controller and page box live as long as the screen. The
// controller is remembered from the screen's own frame call and the box from
// the box's own focus and text calls, so no game pointer outlives its screen:
// the screen keeps refreshing the stamp, and losing it drops both. The
// generation tells two screens apart that a heap reuse put at one address.
struct Screen {
    BookScreenController* controller = nullptr;
    TextEditComponent* box = nullptr;
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
#ifdef LAMIUM_BOOK_PASTE_TRACE
// Defined with the trace hooks further down; the new path's own trace lines
// come first in this file.
std::string sizes(std::string_view text);
void log(std::string line) noexcept;
void noteLimit(TextEditComponent const* box, uint maxLength, bool constrain) noexcept;
#endif

// The page box is the only book text box with the page's character limit and
// the page rectangle's constraint; the title and author take 512 and do not
// constrain.
bool pageBox(TextEditComponent const& box) noexcept {
    return box.mMaxLength == static_cast<uint>(pageLimit) && box.mConstrainToRect;
}
void rememberController(BookScreenController& controller) noexcept {
    std::lock_guard lock(stateMutex);
    if (screen.controller != &controller) {
        screen.controller = &controller;
        screen.box = nullptr;      // another screen's box
        screen.generation = ++generations;
        pending.reset();           // a paste belongs to the screen that made it
    }
    ++screen.frame;
    screen.seen = Clock::now();
}
void rememberBox(TextEditComponent& box) noexcept {
    std::lock_guard lock(stateMutex);
    if (!screen.controller || Clock::now() - screen.seen > screenAlive) return;
    screen.box = &box;
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
    return Current{screen.controller, screen.box, screen.generation, screen.frame};
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
};

// Decides a paste and keeps the overflow for the next book frame. The page model
// is never written here: this runs inside the text edit's own call, and adding
// pages rebuilds the page UI, which invalidates the context being edited (the
// L-259 crash at origin()).
Outcome handlePaste(TextEditContext& context, std::string_view paste) {
    auto at = current();
    auto reject = [&](char const* reason) {
        BOOK_PASTE_TRACE("paste gate={} limit={} box={} editable={} view={}", reason, context.mMaxLength,
                         at && at->box, at ? static_cast<bool>(at->controller->mEditable) : false,
                         at ? static_cast<int>(at->controller->mCurrentView) : -1);
        return Outcome{};
    };
    if (paste.empty()) return reject("empty");
    auto settings = Runtime::instance().snapshot();
    if (!settings->ui.bookPaste) return reject("option");
    if (context.mMaxLength != pageLimit) return reject("limit"); // not the page box L-259 knows
    if (!at) return reject("screen");
    auto& controller = *at->controller;
    if (!at->box) return reject("box");
    if (!controller.mEditable) return reject("not-editable");
    if (controller.mCurrentView != BookScreenController::BookView::Viewing) return reject("view");
    auto& pages = controller.mPages.get();
    int const index = controller.mCurrentPageIndex;
    if (index < 0 || index >= static_cast<int>(pages.size())) return reject("page-index");
    PageContent& page = pages[static_cast<std::size_t>(index)];
    if (!page.mPhotoName.get().empty()) return reject("photo"); // photo pages hold no text
    // The caret and the selection count characters of the box's text. Typing
    // reaches the page model a frame later, so a stored page that is a prefix of
    // the live text is pending typing, and the live text is what the caret
    // counts; unrelated text means the paste goes to the game untouched.
    TextGate const gate = pageTextGate(page.mText.get(), context.mCurrentUtf8Text.get());
    if (!gate.pass) return reject(gate.reason);
    std::string_view const text = gate.text;
    int const chars = codePointCount(text);
    auto const& selection = context.mSelection.get();
    // The game counts characters; a caret past the end means the units or the
    // text changed under us, so the game keeps the paste.
    if (context.mCaret < 0 || context.mCaret > chars || selection.mStartCaretPos > chars
        || selection.mEndCaretPos > chars) {
        BOOK_PASTE_TRACE("paste gate=caret caret={} sel={}:{} chars={}", context.mCaret, selection.mStartCaretPos,
                         selection.mEndCaretPos, chars);
        return Outcome{};
    }
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
    BOOK_PASTE_TRACE("paste gate={}{} page={} caret={} sel={}:{}", gate.reason, gate.pending ? " (typed tail)" : "",
                     index, caret, from, to);
    if (split.kind == SplitKind::Vanilla) {
        BOOK_PASTE_TRACE("paste decision=vanilla page={} caret={} sel={}:{} paste {}", index, caret, from, to,
                         sizes(paste));
        return Outcome{};
    }
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
        BOOK_PASTE_TRACE("paste decision=refused needed={} free={} paste {}", split.needed, split.free, sizes(paste));
        return Outcome{Outcome::Kind::Refused};
    }
    // The page keeps the text before the caret plus the part of the paste that
    // fits; everything else becomes pages the next book frame inserts. Only
    // owned text survives this call.
    Outcome outcome;
    outcome.kind = Outcome::Kind::Insert;
    outcome.head = split.pages[0];
    if (!outcome.head.starts_with(before)) return reject("split"); // the split must keep the page's head
    outcome.insert = outcome.head.substr(before.size());
    outcome.trim = !after.empty();
    DeferredPages plan;
    plan.screen = ScreenId{reinterpret_cast<std::uintptr_t>(at->controller), at->generation};
    plan.frame = at->frame;
    plan.index = index;
    plan.head = outcome.head;
    plan.old = std::string(text);
    plan.extra.assign(std::make_move_iterator(split.pages.begin() + 1), std::make_move_iterator(split.pages.end()));
    BOOK_PASTE_TRACE("paste decision=split page={} caret={} sel={}:{} pages={} needed={} free={} paste {}", index,
                     caret, from, to, split.pages.size(), split.needed, split.free, sizes(paste));
    BOOK_PASTE_TRACE("paste defer index={} frame={} extra={} head {} trim={}", plan.index, plan.frame, plan.extra.size(),
                     sizes(plan.head), outcome.trim);
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
        if (!pending || pending->pages.extra.empty()) return;
        if (!planDue(pending->pages.frame, screen.frame)) return; // queued this frame
        plan = pending->pages;
        pending->pages.extra.clear(); // one attempt only
        id = ScreenId{reinterpret_cast<std::uintptr_t>(&controller), screen.generation};
    }
    auto& pages = controller.mPages.get();
    int const count = static_cast<int>(pages.size());
    std::string_view pageText;
    if (plan->index >= 0 && plan->index < count) pageText = pages[static_cast<std::size_t>(plan->index)].mText.get();
    ApplyPlan const decision = planApply(*plan, id, static_cast<bool>(controller.mEditable), pageText, count);
    if (!decision.apply()) {
        BOOK_PASTE_TRACE("paste apply skipped={} index={} extra={} pages={} text {}", applySkipReason(decision.skip),
                         plan->index, plan->extra.size(), count, sizes(pageText));
        return;
    }
    try {
        for (std::size_t k = 0; k < plan->extra.size(); ++k) {
            controller._addPageAt(decision.index + static_cast<int>(k),
                                  PageContent(std::string(plan->extra[k]), std::nullopt));
        }
        BOOK_PASTE_TRACE("paste apply index={} added={} pages={}->{}", decision.index, decision.add, count,
                         static_cast<int>(controller.mPages.get().size()));
    } catch (std::exception const& error) {
        BOOK_PASTE_TRACE("paste apply failed: {}", error.what());
    } catch (...) { BOOK_PASTE_TRACE("paste apply failed"); }
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
    std::string before;
#ifdef LAMIUM_BOOK_PASTE_TRACE
    try { before = sizes(text); } catch (...) {}
#endif
    bool accepted = origin(std::move(text));
    try { if (accepted && pageBox(*this)) rememberBox(*this); } catch (...) {}
#ifdef LAMIUM_BOOK_PASTE_TRACE
    try {
        noteLimit(this, mMaxLength, mConstrainToRect);
        log(std::format("trySetText {} maxLength={} constrain={} accepted={}", before, mMaxLength,
                        static_cast<bool>(mConstrainToRect), accepted));
    } catch (...) {}
#endif
    return accepted;
}
LL_TYPE_INSTANCE_HOOK(ContextText, ll::memory::HookPriority::Normal, TextEditContext,
    &TextEditContext::feedText, void, std::string_view utf8Text) {
#ifdef LAMIUM_BOOK_PASTE_TRACE
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedText {} caret={} sel={}:{} maxLength={} text {}", sizes(utf8Text), mCaret,
                        selection.mStartCaretPos, selection.mEndCaretPos, mMaxLength, sizes(mCurrentUtf8Text.get())));
    } catch (...) {}
#endif
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
            // box keeps only the first page's text.
            if (outcome.trim) _setText(outcome.head);
            break;
        case Outcome::Kind::Refused: break;
        }
    } catch (...) {
        cancelPending();
#ifdef LAMIUM_BOOK_PASTE_TRACE
        try { log("paste insert failed"); } catch (...) {}
#endif
    }
    insidePaste = false;
#ifdef LAMIUM_BOOK_PASTE_TRACE
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedText done caret={} sel={}:{} text {}", mCaret, selection.mStartCaretPos,
                        selection.mEndCaretPos, sizes(mCurrentUtf8Text.get())));
    } catch (...) {}
#endif
}
#ifdef LAMIUM_BOOK_PASTE_TRACE
// The trace's own helpers: one window for every stage, so a whole book pasted
// into a full page cannot push more than this many lines into the log.
class Window {
    std::atomic<long long> start{-1};
    std::atomic<unsigned> used{0};
public:
    static constexpr unsigned limit = 300;
    static constexpr long long span = 5000;
    bool take() noexcept {
        auto elapsed = [] {
            return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
        };
        long long now = elapsed();
        long long since = start.load(std::memory_order_relaxed);
        if (since < 0 || now - since >= span) {
            if (start.compare_exchange_strong(since, now, std::memory_order_relaxed)) used.store(0, std::memory_order_relaxed);
        }
        return used.fetch_add(1, std::memory_order_relaxed) < limit;
    }
};
Window window;
// Diagnostics must never interrupt the hooked call: errors stay swallowed and
// the budget never throws.
void log(std::string line) noexcept {
    if (!window.take()) return;
    try { Runtime::instance().self().getLogger().info("book paste trace: {} tid={}", line, GetCurrentThreadId()); } catch (...) {}
}
// The two units L-259 has to tell apart.
std::string sizes(std::string_view text) { return std::format("bytes={} chars={}", text.size(), codePointCount(text)); }
char const* actionName(TextEditContext::TextEditAction action) {
    switch (action) {
    case TextEditContext::TextEditAction::Backspace: return "backspace";
    case TextEditContext::TextEditAction::Copy: return "copy";
    case TextEditContext::TextEditAction::Cut: return "cut";
    case TextEditContext::TextEditAction::Delete: return "delete";
    case TextEditContext::TextEditAction::Newline: return "newline";
    case TextEditContext::TextEditAction::Paste: return "paste";
    case TextEditContext::TextEditAction::SelectAll: return "selectAll";
    }
    return "unknown";
}
char const* operationName(size_t index) {
    switch (index) {
    case 0: return "replace";
    case 1: return "add";
    case 2: return "delete";
    case 3: return "swap";
    case 4: return "finalize";
    }
    return "unknown";
}
// The page text box's limit, once per box: the JSON variable itself is not in
// the SDK (Json::Value is forward declared only), so the resolved component
// value is the readable form.
void noteLimit(TextEditComponent const* box, uint maxLength, bool constrain) noexcept {
    static thread_local TextEditComponent const* lastBox = nullptr;
    static thread_local uint lastLimit = 0;
    if (box == lastBox && maxLength == lastLimit) return;
    lastBox = box;
    lastLimit = maxLength;
    try { log(std::format("page limit maxLength={} constrain={}", maxLength, constrain)); } catch (...) {}
}
std::string pageText(std::vector<PageContent>& pages, int index) {
    return index >= 0 && index < static_cast<int>(pages.size()) ? sizes(pages[index].mText.get()) : std::string("page=none");
}
LL_TYPE_INSTANCE_HOOK(TextCharKey, ll::memory::HookPriority::Normal, TextEditComponent,
    &TextEditComponent::handleTextCharEvent, TextCharEventResult, VisualTree& visualTree, std::string const& inputUtf8) {
    TextCharEventResult result = origin(visualTree, inputUtf8);
    try {
        noteLimit(this, mMaxLength, mConstrainToRect);
        log(std::format("key {} maxLength={} constrain={} result={}", sizes(inputUtf8), mMaxLength,
                        static_cast<bool>(mConstrainToRect), static_cast<int>(result)));
    } catch (...) {}
    return result;
}
LL_TYPE_INSTANCE_HOOK(BookScreenOpen, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::$addStaticScreenVars, void, Json::Value& globalVars) {
    try {
        log(std::format("screen open editable={} pages={} page={} view={}", static_cast<bool>(mEditable),
                        mPages.get().size(), mCurrentPageIndex, static_cast<int>(mCurrentView)));
    } catch (...) {}
    origin(globalVars);
}
LL_TYPE_INSTANCE_HOOK(TextFits, ll::memory::HookPriority::Normal, TextEditComponent,
    &TextEditComponent::_textFitsInControl, bool, std::string const& text) {
    bool fits = origin(text);
    try {
        noteLimit(this, mMaxLength, mConstrainToRect);
        log(std::format("fits {} maxLength={} constrain={} fits={}", sizes(text), mMaxLength,
                        static_cast<bool>(mConstrainToRect), fits));
    } catch (...) {}
    return fits;
}
LL_TYPE_INSTANCE_HOOK(ContextAction, ll::memory::HookPriority::Normal, TextEditContext,
    &TextEditContext::feedAction, void, TextEditAction action) {
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedAction action={}({}) caret={} sel={}:{} maxLength={} text {}", static_cast<int>(action),
                        actionName(action), mCaret, selection.mStartCaretPos, selection.mEndCaretPos, mMaxLength,
                        sizes(mCurrentUtf8Text.get())));
    } catch (...) {}
    origin(action);
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedAction done action={}({}) caret={} sel={}:{} text {}", static_cast<int>(action),
                        actionName(action), mCaret, selection.mStartCaretPos, selection.mEndCaretPos,
                        sizes(mCurrentUtf8Text.get())));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(PageParse, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::_parsePageText, void, int index) {
    origin(index);
    try {
        auto& pages = mPages.get();
        log(std::format("parsePage index={} pages={} {}", index, pages.size(), pageText(pages, index)));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(PageSetText, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::_setPageText, void, int index, std::string const& text) {
    try { log(std::format("setPage index={} pages={} {}", index, mPages.get().size(), sizes(text))); } catch (...) {}
    origin(index, text);
    try {
        auto& pages = mPages.get();
        log(std::format("setPage stored index={} pages={} {}", index, pages.size(), pageText(pages, index)));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(PageAddAt, ll::memory::HookPriority::Normal, BookScreenController,
    &BookScreenController::_addPageAt, void, int index, PageContent const& page) {
    try {
        log(std::format("addPage index={} pages={} {} photo={}", index, mPages.get().size(), sizes(page.mText.get()),
                        page.mPhotoName.get().size()));
    } catch (...) {}
    origin(index, page);
    try { log(std::format("addPage stored index={} pages={}", index, mPages.get().size())); } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(ManagerEditPage, ll::memory::HookPriority::Normal, BookScreenManager,
    &BookScreenManager::editPage, void, int index, PageContent const& page) {
    try {
        log(std::format("manager editPage index={} slot={} edited={} {}", index, mBookSlot, static_cast<bool>(mEdited),
                        sizes(page.mText.get())));
    } catch (...) {}
    origin(index, page);
}
LL_TYPE_INSTANCE_HOOK(ManagerAddPage, ll::memory::HookPriority::Normal, BookScreenManager,
    &BookScreenManager::addPageAt, void, int index, PageContent const& page) {
    try {
        log(std::format("manager addPageAt index={} slot={} edited={} {}", index, mBookSlot,
                        static_cast<bool>(mEdited), sizes(page.mText.get())));
    } catch (...) {}
    origin(index, page);
}
// Only the fields the packet header declares as plain values are read; the
// operation variant is inspected by index, so an unknown one logs as unknown.
void logBookEdit(char const* channel, Packet& packet) noexcept {
    try {
        auto const& edit = static_cast<BookEditPacket&>(packet);
        auto const& payload = static_cast<BookEditPacketPayload const&>(edit);
        auto const& operation = payload.mOperation.get();
        int index = -1;
        std::string text;
        switch (operation.index()) {
        case 0: {
            auto const& page = std::get<BookEditAction::ReplacePage>(operation);
            index = page.mPageIndex;
            text = page.mPageText.get();
            break;
        }
        case 1: {
            auto const& page = std::get<BookEditAction::AddPage>(operation);
            index = page.mPageIndex;
            text = page.mPageText.get();
            break;
        }
        case 2: index = std::get<BookEditAction::DeletePage>(operation).mPageIndex; break;
        case 3: index = std::get<BookEditAction::SwapPages>(operation).mPageIndex; break;
        default: break;
        }
        log(std::format("packet via={} slot={} action={} page={} bytes={} chars={}", channel, payload.mBookSlot,
                        operationName(operation.index()), index, text.size(), codePointCount(text)));
    } catch (...) {}
}
LL_TYPE_INSTANCE_HOOK(BookEditSend, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$send, void, Packet& packet) {
    if (packet.getId() == MinecraftPacketIds::BookEdit) logBookEdit("send", packet);
    origin(packet);
}
LL_TYPE_INSTANCE_HOOK(BookEditSendToServer, ll::memory::HookPriority::Normal, LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer, void, Packet& packet) {
    if (packet.getId() == MinecraftPacketIds::BookEdit) logBookEdit("sendToServer", packet);
    origin(packet);
}
#endif
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
#ifdef LAMIUM_BOOK_PASTE_TRACE
    {"BookScreenOpen", BookScreenOpen::hook, BookScreenOpen::unhook},
    {"TextCharKey", TextCharKey::hook, TextCharKey::unhook},
    {"TextFits", TextFits::hook, TextFits::unhook},
    {"ContextAction", ContextAction::hook, ContextAction::unhook},
    {"PageParse", PageParse::hook, PageParse::unhook},
    {"PageSetText", PageSetText::hook, PageSetText::unhook},
    {"PageAddAt", PageAddAt::hook, PageAddAt::unhook},
    {"ManagerEditPage", ManagerEditPage::hook, ManagerEditPage::unhook},
    {"ManagerAddPage", ManagerAddPage::hook, ManagerAddPage::unhook},
    {"BookEditSend", BookEditSend::hook, BookEditSend::unhook},
    {"BookEditSendToServer", BookEditSendToServer::hook, BookEditSendToServer::unhook},
#endif
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
#ifdef LAMIUM_BOOK_PASTE_TRACE
    try {
        Runtime::instance().self().getLogger().warn(
            "L-259 book paste trace enabled: {} hooks; lines start with 'book paste trace: ', at most {} per {} ms",
            static_cast<unsigned>(sizeof(hooks) / sizeof(hooks[0])), Window::limit, Window::span);
    } catch (...) {}
#endif
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
