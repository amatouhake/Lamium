#include "features/inventory/BookPaste.h"
#ifdef LAMIUM_BOOK_PASTE_TRACE
#include "app/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/gui/controls/TextEditComponent.h"
#include "mc/client/gui/screens/controllers/BookScreenController.h"
#include "mc/client/gui/screens/managers/BookScreenManager.h"
#include "mc/deps/input/TextEditContext.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/packet/BookEditPacket.h"
#include "mc/world/item/PageContent.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <variant>

namespace lamium::inventory::bookPaste {
namespace {
using Clock = std::chrono::steady_clock;
// One window for every stage, so a whole book pasted into a full page cannot
// push more than this many lines into the log in five seconds.
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
LL_TYPE_INSTANCE_HOOK(TextTrySet, ll::memory::HookPriority::Normal, TextEditComponent,
    &TextEditComponent::trySetText, bool, std::string text) {
    std::string before;
    try { before = sizes(text); } catch (...) {}
    bool accepted = origin(std::move(text));
    try {
        noteLimit(this, mMaxLength, mConstrainToRect);
        log(std::format("trySetText {} maxLength={} constrain={} accepted={}", before, mMaxLength,
                        static_cast<bool>(mConstrainToRect), accepted));
    } catch (...) {}
    return accepted;
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
LL_TYPE_INSTANCE_HOOK(ContextText, ll::memory::HookPriority::Normal, TextEditContext,
    &TextEditContext::feedText, void, std::string_view utf8Text) {
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedText {} caret={} sel={}:{} maxLength={} text {}", sizes(utf8Text), mCaret,
                        selection.mStartCaretPos, selection.mEndCaretPos, mMaxLength, sizes(mCurrentUtf8Text.get())));
    } catch (...) {}
    origin(utf8Text);
    try {
        auto const& selection = mSelection.get();
        log(std::format("feedText done caret={} sel={}:{} text {}", mCaret, selection.mStartCaretPos,
                        selection.mEndCaretPos, sizes(mCurrentUtf8Text.get())));
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
struct Hook {
    char const* name;
    int (*install)(bool);
    bool (*remove)(bool);
    bool installed = false;
};
Hook hooks[] = {
    {"BookScreenOpen", BookScreenOpen::hook, BookScreenOpen::unhook},
    {"TextCharKey", TextCharKey::hook, TextCharKey::unhook},
    {"TextTrySet", TextTrySet::hook, TextTrySet::unhook},
    {"TextFits", TextFits::hook, TextFits::unhook},
    {"ContextAction", ContextAction::hook, ContextAction::unhook},
    {"ContextText", ContextText::hook, ContextText::unhook},
    {"PageParse", PageParse::hook, PageParse::unhook},
    {"PageSetText", PageSetText::hook, PageSetText::unhook},
    {"PageAddAt", PageAddAt::hook, PageAddAt::unhook},
    {"ManagerEditPage", ManagerEditPage::hook, ManagerEditPage::unhook},
    {"ManagerAddPage", ManagerAddPage::hook, ManagerAddPage::unhook},
    {"BookEditSend", BookEditSend::hook, BookEditSend::unhook},
    {"BookEditSendToServer", BookEditSendToServer::hook, BookEditSendToServer::unhook},
};
}
void startBookPaste() {
    // Each hook attaches on its own: a missing one narrows the trace instead of
    // stopping it, and the summary names what did not attach.
    unsigned attached = 0, failed = 0;
    for (auto& hook : hooks) {
        if (hook.installed) {
            ++attached;
            continue;
        }
        try {
            if (hook.install(true) != 0) {
                ++failed;
                try {
                    Runtime::instance().self().getLogger().warn("L-259 book paste trace: {} did not attach", hook.name);
                } catch (...) {}
                continue;
            }
            hook.installed = true;
            ++attached;
        } catch (std::exception const& error) {
            ++failed;
            try {
                Runtime::instance().self().getLogger().warn("L-259 book paste trace: {} did not attach: {}", hook.name,
                                                            error.what());
            } catch (...) {}
        } catch (...) { ++failed; }
    }
    try {
        Runtime::instance().self().getLogger().warn(
            "L-259 book paste trace enabled: {}/{} hooks attached, {} failed; lines start with 'book paste trace: ', "
            "at most {} per {} ms",
            attached, static_cast<unsigned>(sizeof(hooks) / sizeof(hooks[0])), failed, Window::limit, Window::span);
    } catch (...) {}
}
void stopBookPaste() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it) {
        if (!it->installed) continue;
        try {
            if (it->remove(true)) it->installed = false;
        } catch (...) {}
    }
}
}
#else
namespace lamium::inventory::bookPaste {
void startBookPaste() {}
void stopBookPaste() {}
}
#endif
