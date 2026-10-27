#pragma once
#include <cstddef>
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
}
