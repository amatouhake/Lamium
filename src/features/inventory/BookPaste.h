#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace lamium::inventory::bookPaste {
// L-259: a paste into a book and quill page that does not fit continues on new
// pages. Where a page ends and which text lands on each is decided here,
// without game types, and covered by tests/BookPasteTests.cpp. BookPaste.cpp
// feeds it the game's own page limit and text-fit test, so wrapping and fonts
// need no copy in Lamium.
inline constexpr int pageLimit = 256; // characters the vanilla page box takes
inline constexpr int maxPages = 50;   // pages a book and quill holds

bool start();
void stop();

// UTF-8 sequences, which is what the game counts as characters; bytes are the
// other unit L-259 has to tell apart (Japanese is three bytes per character).
constexpr int codePointCount(std::string_view text) {
    int count = 0;
    for (unsigned char c : text) if ((c & 0xc0) != 0x80) ++count;
    return count;
}

// Byte offset of a code-point index: clamped to the text and never inside a
// UTF-8 sequence.
inline std::size_t byteOffset(std::string_view text, int codePoints) noexcept {
    if (codePoints <= 0) return 0;
    int index = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xc0) == 0x80) continue;
        if (index == codePoints) return i;
        ++index;
    }
    return text.size();
}

// Where one page ends: code points kept on it, and code points dropped because
// they are the break itself (a line break or a space, so the next page does not
// start blank).
struct Break {
    int keep = 0;
    int drop = 0;
};
// The page break inside the part of `text` that fits (`maxFit` code points):
// after the last line break, else after the last space, unless that leaves the
// page less than half full; otherwise at the last character that fits.
inline Break chooseBreak(std::string_view text, int maxFit) noexcept {
    Break result;
    if (maxFit <= 0) return result;
    int lineBreak = -1, space = -1, index = 0, previous = 0;
    bool lineBreakEndsPair = false;
    for (std::size_t i = 0; i < text.size() && index <= maxFit; ++i) {
        unsigned char const byte = static_cast<unsigned char>(text[i]);
        if ((byte & 0xc0) == 0x80) continue; // a continuation byte is not a character
        if (byte == '\n' || byte == '\r') {
            lineBreak = index;
            lineBreakEndsPair = byte == '\n' && previous == '\r';
        } else if (byte == ' ') {
            space = index;
        }
        previous = byte;
        ++index;
    }
    int keep = -1, drop = 0;
    if (lineBreak >= 0) {
        keep = lineBreak, drop = 1;
        if (lineBreakEndsPair && lineBreak > 0) {
            keep = lineBreak - 1;
            drop = 2;
        }
    } else if (space >= 0) {
        keep = space;
        drop = 1;
    }
    // A break that wastes more than half the room is worse than a plain cut.
    if (keep < 0 || keep * 2 < maxFit) {
        keep = maxFit;
        drop = 0;
    }
    result.keep = keep;
    result.drop = drop;
    return result;
}

enum class SplitKind {
    Vanilla, // the whole paste fits the page: leave it to the game
    Pages,   // pages[0] replaces the current page, the rest are new pages after it
    Refused, // more pages than are free: paste nothing
};
struct SplitResult {
    SplitKind kind = SplitKind::Vanilla;
    std::vector<std::string> pages;
    int needed = 0; // Refused: new pages the paste needs
    int free = 0;   // Refused: new pages that were free
};
namespace detail {
// Largest code-point prefix of `text` whose page `prefix + prefixOf(text) +
// suffix` the game accepts. A test that is not monotone at a page boundary can
// only make this smaller, never keep text the game refuses.
template<class Fits>
int largestFit(std::string_view prefix, std::string_view text, std::string_view suffix, int limit, Fits& fits) {
    int const chars = codePointCount(text);
    int high = chars < limit ? chars : limit;
    if (high <= 0) return 0;
    auto page = [&](int count) {
        std::string candidate;
        candidate.reserve(prefix.size() + text.size() + suffix.size());
        candidate.append(prefix);
        candidate.append(text.substr(0, byteOffset(text, count)));
        candidate.append(suffix);
        return fits(candidate);
    };
    if (page(high)) return high;
    int low = 0;
    while (high - low > 1) {
        int const middle = low + (high - low) / 2;
        if (page(middle)) low = middle;
        else high = middle;
    }
    while (low > 0 && !page(low)) --low;
    return low;
}
}
// Splits a paste that does not fit the page. `before` is the page text up to
// the insertion point, `after` the text from the end of the selection on,
// `paste` the clipboard text, and `fits` the game's own test of a whole page's
// text. `limit` only bounds the search; how many characters the game accepts
// comes from `fits`.
template<class Fits>
SplitResult splitPaste(std::string_view before, std::string_view after, std::string_view paste,
                       int freePages, int limit, Fits&& fits) {
    SplitResult result;
    result.free = freePages < 0 ? 0 : freePages;
    if (paste.empty() || limit <= 0) return result;
    auto& fitsPage = fits;
    std::string whole;
    whole.reserve(before.size() + paste.size() + after.size());
    whole.append(before).append(paste).append(after);
    if (fitsPage(whole)) return result;
    // The current page keeps the text before the insertion point and as much of
    // the paste as fits.
    Break const first = chooseBreak(paste, detail::largestFit(before, paste, {}, limit, fitsPage));
    std::string current(before);
    current.append(paste.substr(0, byteOffset(paste, first.keep)));
    result.pages.push_back(std::move(current));
    std::string rest(paste.substr(byteOffset(paste, first.keep + first.drop)));
    std::string tail(after);
    // Far past the book's limit the exact count cannot help anyone: stop
    // splitting megabytes of clipboard text.
    int const pageCap = maxPages * 4;
    for (;;) {
        std::string both(rest);
        both.append(tail);
        if (fitsPage(both)) {
            result.pages.push_back(std::move(both));
            break;
        }
        Break next;
        if (!rest.empty()) next = chooseBreak(rest, detail::largestFit({}, rest, {}, limit, fitsPage));
        if (next.keep <= 0 || static_cast<int>(result.pages.size()) >= pageCap) {
            // Nothing more fits on a page, or the paste is far too long to
            // place: refuse rather than paste part of it.
            result.kind = SplitKind::Refused;
            result.needed = static_cast<int>(result.pages.size());
            result.pages.clear();
            return result;
        }
        result.pages.push_back(rest.substr(0, byteOffset(rest, next.keep)));
        rest.erase(0, byteOffset(rest, next.keep + next.drop));
    }
    result.kind = SplitKind::Pages;
    result.needed = static_cast<int>(result.pages.size()) - 1;
    if (result.needed > result.free) {
        result.kind = SplitKind::Refused;
        result.pages.clear();
    }
    return result;
}

// The page box's text and the page model's text can differ for a frame after
// typing (the model is parsed from the box later). A stored page that is a
// prefix of the live text is that state: the extra tail is pending typing, and
// the caret and the selection count the live text. Unrelated text means the
// caret cannot be predicted, so the game keeps the paste.
struct TextGate {
    bool pass = false;
    bool pending = false;      // the live text extends the stored page
    char const* reason = "text-unrelated";
    std::string_view text{};   // the text the caret and the selection count
};
inline TextGate pageTextGate(std::string_view stored, std::string_view live) noexcept {
    if (!live.starts_with(stored)) return TextGate{};
    bool const pending = stored.size() != live.size();
    return TextGate{true, pending, pending ? "pending-typing" : "ok", live};
}

// Which page the text box belongs to. A two-page spread keeps its own idea of
// the current page, and after the player edits the other half of the spread it
// can still name the page they just left (seen in game 2026-10-27: a paste on
// the left page after editing the right one fell back to vanilla). The page is
// therefore found from the text the box holds: the preferred page when it
// matches, otherwise the page whose stored text matches, exactly or as pending
// typing, nearest to the preferred one and longest first.
struct PageChoice {
    int index = -1;
    char const* reason = "page-text";
    bool ok() const { return index >= 0; }
};
inline PageChoice choosePastedPage(std::string_view live, int preferred,
                                   std::vector<std::string_view> const& stored) noexcept {
    auto const count = static_cast<int>(stored.size());
    auto const matches = [&](int i, bool exactOnly) {
        return pageTextGate(stored[static_cast<std::size_t>(i)], live).pass
            && (!exactOnly || stored[static_cast<std::size_t>(i)].size() == live.size());
    };
    if (preferred >= 0 && preferred < count && matches(preferred, false)) return PageChoice{preferred, "ok"};
    auto const closest = [&](bool exactOnly) {
        int best = -1;
        std::size_t bestLength = 0;
        for (int i = 0; i < count; ++i) {
            if (!matches(i, exactOnly)) continue;
            std::size_t const length = stored[static_cast<std::size_t>(i)].size();
            bool const better = best < 0 || length > bestLength
                || (length == bestLength && std::abs(i - preferred) < std::abs(best - preferred));
            if (better) { best = i; bestLength = length; }
        }
        return best;
    };
    int const exact = closest(true);
    if (exact >= 0) return PageChoice{exact, "page-text"};
    int const prefix = closest(false);
    if (prefix >= 0) return PageChoice{prefix, "pending-typing"};
    return PageChoice{};
}

// The book screen a deferred paste belongs to: the controller address plus the
// generation the frame hook bumps when the controller changes, so a new screen
// at the same address is not the old one.
struct ScreenId {
    std::uintptr_t controller = 0;
    unsigned long long generation = 0;
    bool operator==(ScreenId const&) const = default;
};

// A paste's overflow, held as owned text until the book screen's next frame.
// `index` is the page the first chunk went to, `head` the text that page must
// hold, `old` the page text when the paste happened (the page model is parsed
// a frame later, so it can still hold that or a prefix of it), `frame` the frame
// the paste arrived on, and `extra` the pages to insert after `index` in order.
struct DeferredPages {
    ScreenId screen{};
    unsigned long long frame = 0;
    int index = 0;
    std::string head;
    std::string old;
    std::vector<std::string> extra;
};

// A deferred paste or its refusal waits for a frame after the paste: writing the
// page model inside the text edit's own call is what crashed L-259.
inline bool planDue(unsigned long long queuedFrame, unsigned long long frame) noexcept {
    return frame > queuedFrame;
}

// Whether the page still holds what the paste left on it: the new text, or the
// text it had when the paste happened. The page model is parsed from the box a
// frame later, so the old text, or a prefix of it while typing is pending, is
// that lag and not a different page.
inline bool pageUnchanged(DeferredPages const& plan, std::string_view pageText) noexcept {
    return pageText == plan.head || plan.old.starts_with(pageText);
}

enum class ApplySkip {
    None,
    Empty,    // nothing to add
    Screen,   // another book screen
    Editable, // the book is no longer editable
    Page,     // the index is not a page any more
    Moved,    // the page holds text the paste did not write
    Room,     // the 50-page limit would be passed
};
// The word the trace prints for a skipped application.
inline char const* applySkipReason(ApplySkip skip) noexcept {
    switch (skip) {
    case ApplySkip::None: return "none";
    case ApplySkip::Empty: return "empty";
    case ApplySkip::Screen: return "screen";
    case ApplySkip::Editable: return "not-editable";
    case ApplySkip::Page: return "page";
    case ApplySkip::Moved: return "page-moved";
    case ApplySkip::Room: return "no-room";
    }
    return "unknown";
}
struct ApplyPlan {
    ApplySkip skip = ApplySkip::None;
    int index = 0; // where the pages go
    int add = 0;   // pages to insert, in order
    bool apply() const noexcept { return skip == ApplySkip::None; }
};
// Whether the pages kept for a paste may be inserted now: the same book screen,
// still editable, the page still holding what the paste left there, and room
// under the 50-page limit. Decided from owned values and the caller's live
// readings only.
inline ApplyPlan planApply(DeferredPages const& plan, ScreenId const& current, bool editable,
                           std::string_view pageText, int pageCount) noexcept {
    ApplyPlan result;
    int const extra = static_cast<int>(plan.extra.size());
    if (extra <= 0) {
        result.skip = ApplySkip::Empty;
    } else if (!(current == plan.screen)) {
        result.skip = ApplySkip::Screen;
    } else if (!editable) {
        result.skip = ApplySkip::Editable;
    } else if (plan.index < 0 || plan.index >= pageCount) {
        result.skip = ApplySkip::Page;
    } else if (!pageUnchanged(plan, pageText)) {
        result.skip = ApplySkip::Moved;
    } else if (pageCount + extra > maxPages) {
        result.skip = ApplySkip::Room;
    } else {
        result.index = plan.index;
        result.add = extra;
    }
    return result;
}
}
