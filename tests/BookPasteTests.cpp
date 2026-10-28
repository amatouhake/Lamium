#include "features/inventory/BookPaste.h"
#include <string>
#include <string_view>
#include <vector>

void check(bool, char const*);

namespace {
using namespace lamium::inventory::bookPaste;
// A page that counts characters only, like the game's own limit; the real test
// is the page box's.
auto fitsUpTo(int limit) {
    return [limit](std::string const& text) { return codePointCount(text) <= limit; };
}
auto pagesText(std::vector<std::string> const& pages) {
    std::string joined;
    for (auto const& page : pages) joined += page;
    return joined;
}
}

void bookPasteTests() {
    using lamium::inventory::bookPaste::codePointCount;
    check(codePointCount("") == 0, "empty text has no code points");
    check(codePointCount("book") == 4, "ASCII counts one code point per byte");
    check(std::string("本の栞").size() == 9 && codePointCount("本の栞") == 3,
          "three Japanese characters are nine bytes but three code points");
    check(codePointCount("A🌿日本") == 4, "a four-byte character counts once");
    check(codePointCount("\xe6\x9c\xac") == 1, "one three-byte sequence counts once");
    check(codePointCount("\xe6\x9c") == 1, "a truncated sequence still counts the bytes it has");

    // Code-point cuts land on character boundaries; a byte index that would
    // split a sequence becomes the start of the next character or the end.
    check(byteOffset("book", 0) == 0 && byteOffset("book", 2) == 2 && byteOffset("book", 9) == 4,
          "a byte offset clamps to the text");
    check(byteOffset("本の栞", 1) == 3 && byteOffset("本の栞", 2) == 6 && byteOffset("本の栞", 3) == 9,
          "a Japanese character is three bytes, cut between characters");
    check(byteOffset("A🌿B", 1) == 1 && byteOffset("A🌿B", 2) == 5 && byteOffset("A🌿B", 3) == 6,
          "a four-byte character counts once in a cut");
    check(byteOffset("\xe6\x9c", 1) == 2, "a truncated sequence ends at its last byte");

    // The break rule: line break, else space, unless less than half the room.
    {
        Break space = chooseBreak("hello world", 8);
        check(space.keep == 5 && space.drop == 1, "a page breaks after the last space");
        Break shortest = chooseBreak("hello world", 11);
        check(shortest.keep == 11 && shortest.drop == 0, "a break below half the room cuts at the character instead");
        Break tiny = chooseBreak("a bbbbbbbb", 8);
        check(tiny.keep == 8 && tiny.drop == 0, "a space just after the page start is not a page break");
        Break line = chooseBreak("ab\ncd ef", 4);
        check(line.keep == 2 && line.drop == 1, "a line break wins over a later space and is dropped");
        Break both = chooseBreak("ab cd\nef gh", 6);
        check(both.keep == 5 && both.drop == 1, "the last line break wins over an earlier space");
        Break windows = chooseBreak("ab\r\ncd", 3);
        check(windows.keep == 2 && windows.drop == 2, "a CRLF line break drops both characters");
        Break edge = chooseBreak("abc\r\nd", 3);
        check(edge.keep == 3 && edge.drop == 2, "a CRLF whose CR is the first character past the page drops its LF too");
        Break carriage = chooseBreak("ab\rcd", 3);
        check(carriage.keep == 2 && carriage.drop == 1, "a lone carriage return ends the page");
        Break none = chooseBreak("本の栞です", 3);
        check(none.keep == 3 && none.drop == 0, "Japanese without spaces breaks by character");
        Break empty = chooseBreak("abc", 0);
        check(empty.keep == 0 && empty.drop == 0, "no room, nothing kept");
    }

    // The split itself: pages[0] is the current page, the rest go after it.
    {
        auto split = splitPaste("ab", "cd", "ef", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Vanilla && split.pages.empty(), "a paste that fits is left to the game");
    }
    {
        auto split = splitPaste("", "", "abcdefghij", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Pages && split.pages.size() == 2 && split.pages[0] == "abcdefgh"
              && split.pages[1] == "ij" && split.needed == 1, "a long paste fills the page and continues");
    }
    {
        auto split = splitPaste("hi ", "!", "abcdefghij", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Pages && split.pages.size() == 2 && split.pages[0] == "hi abcde"
              && split.pages[1] == "fghij!", "the text after the caret follows the paste onto the new page");
    }
    {
        auto split = splitPaste("", "", "aaaa bbbb cccc", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Pages && split.pages.size() == 3 && split.pages[0] == "aaaa"
              && split.pages[1] == "bbbb" && split.pages[2] == "cccc",
              "each page breaks after its last space, and the space is dropped");
    }
    {
        auto split = splitPaste("", "", "a bbbbbbbbbbbb", 5, 8, fitsUpTo(8));
        check(split.pages.size() == 2 && split.pages[0] == "a bbbbbb" && split.pages[1] == "bbbbbb",
              "a page only half full is not cut at the space");
    }
    {
        auto split = splitPaste("12345678", "", "abcdef", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Pages && split.pages.size() == 2 && split.pages[0] == "12345678"
              && split.pages[1] == "abcdef", "a paste into a full page starts on a new page, keeping the page");
    }
    {
        auto split = splitPaste("", "", "本の栞です", 5, 3, fitsUpTo(3));
        check(split.kind == SplitKind::Pages && split.pages.size() == 2 && split.pages[0] == "本の栞"
              && split.pages[1] == "です" && split.pages[0].size() == 9,
              "a Japanese paste is cut between characters, never inside one");
        check(codePointCount(split.pages[0]) == 3 && codePointCount(split.pages[1]) == 2,
              "Japanese pages hold whole characters");
    }
    {
        // 300 characters on a 256-character page: the real limits of L-259.
        std::string paste(300, 'a');
        auto split = splitPaste("", "", paste, 5, pageLimit, fitsUpTo(pageLimit));
        check(split.kind == SplitKind::Pages && split.pages.size() == 2 && split.pages[0].size() == 256
              && split.pages[1].size() == 44 && split.needed == 1 && split.free == 5,
              "a 300-character paste fills the 256-character page and continues once");
    }
    {
        std::string paste(30, 'x');
        auto split = splitPaste("", "", paste, 2, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Refused && split.needed == 3 && split.free == 2 && split.pages.empty(),
              "more pages than are free refuses the whole paste and counts them");
    }
    {
        std::string paste(300, 'a');
        auto split = splitPaste("", "", paste, 0, pageLimit, fitsUpTo(pageLimit));
        check(split.kind == SplitKind::Refused && split.needed == 1 && split.free == 0 && split.pages.empty(),
              "no free page refuses rather than pasting part of the text");
    }
    {
        auto split = splitPaste("", "", "", 5, 8, fitsUpTo(8));
        check(split.kind == SplitKind::Vanilla && split.pages.empty(), "an empty paste is left to the game");
    }
    // Every page the split plans must be one the game accepts, and no text may
    // be lost or invented except the dropped break characters.
    {
        std::string const before = "Intro: ";
        std::string const after = " (end)";
        std::string paste;
        for (int i = 0; i < 40; ++i) paste += "word" + std::to_string(i) + (i % 7 == 6 ? "\n" : " ");
        auto fits = fitsUpTo(24);
        auto split = splitPaste(before, after, paste, 50, 24, fits);
        bool allFit = split.kind == SplitKind::Pages;
        for (auto const& page : split.pages) allFit = allFit && fits(page);
        check(allFit, "every planned page fits the game's test");
        check(split.pages.front().starts_with(before) && split.pages.back().ends_with(after),
              "the first page keeps the text before the caret and the last the text after it");
        std::string const joined = pagesText(split.pages);
        check(joined.size() < before.size() + paste.size() + after.size(),
              "only the break characters between pages are dropped");
        auto compact = [](std::string const& text) {
            std::string out;
            for (char c : text) if (c != ' ' && c != '\n') out += c;
            return out;
        };
        check(compact(joined) == compact(before + paste + after), "no other character of the paste is lost");
    }

    // The page-text gate: typing reaches the page model a frame later, so a
    // stored page that is a prefix of the live text is pending typing.
    {
        auto pending = pageTextGate("hello", "hello world");
        check(pending.pass && pending.pending && pending.text == "hello world",
              "a stored page that the live text extends passes the gate as pending typing");
        check(std::string(pending.reason) == "pending-typing", "pending typing names itself in the trace");
        auto same = pageTextGate("hello", "hello");
        check(same.pass && !same.pending && same.text == "hello", "equal page texts pass the gate unchanged");
        auto empty = pageTextGate("", "typed");
        check(empty.pass && empty.pending && empty.text == "typed",
              "an empty page model is a prefix, so the first typed characters pass");
        auto both = pageTextGate("", "");
        check(both.pass && !both.pending, "an empty page and an empty box pass");
        auto unrelated = pageTextGate("hello", "goodbye");
        check(!unrelated.pass && std::string(unrelated.reason) == "text-unrelated",
              "unrelated page text is refused and says why");
        check(!pageTextGate("hello", "").pass, "a live text shorter than the page is refused");
        check(!pageTextGate("hello world", "hello").pass, "a live text that is a prefix of the stored page is refused");
        check(!pageTextGate("hello", "xhello").pass, "text inserted before the stored page is unrelated");
        check(!pageTextGate("hello", "helo world").pass, "text typed inside the stored page is unrelated");
    }

    // The deferred plan: what to add, and when it may be applied.
    {
        DeferredPages plan;
        plan.screen = ScreenId{0x1000, 7};
        plan.frame = 40;
        plan.index = 2;
        plan.head = "hello world";
        plan.old = "hello";
        plan.extra = {"abc", "def"};

        auto applied = planApply(plan, ScreenId{0x1000, 7}, true, "hello world", 5);
        check(applied.apply() && applied.index == 2 && applied.add == 2,
              "the same screen, still editable, adds every kept page after the index");

        auto changed = planApply(plan, ScreenId{0x2000, 7}, true, "hello world", 5);
        check(!changed.apply() && std::string(applySkipReason(changed.skip)) == "screen",
              "apply refused when the screen changed");
        auto reused = planApply(plan, ScreenId{0x1000, 8}, true, "hello world", 5);
        check(!reused.apply() && std::string(applySkipReason(reused.skip)) == "screen",
              "apply refused when a new screen reuses the controller address");
        auto readOnly = planApply(plan, ScreenId{0x1000, 7}, false, "hello world", 5);
        check(!readOnly.apply() && std::string(applySkipReason(readOnly.skip)) == "not-editable",
              "apply refused when the book stopped being editable");
        auto moved = planApply(plan, ScreenId{0x1000, 7}, true, "goodbye", 5);
        check(!moved.apply() && std::string(applySkipReason(moved.skip)) == "page-moved",
              "apply refused when the page text moved");
        auto lagging = planApply(plan, ScreenId{0x1000, 7}, true, "hello", 5);
        check(lagging.apply(), "apply allowed while the page model is one parse behind");
        auto laggingPrefix = planApply(plan, ScreenId{0x1000, 7}, true, "help", 5);
        check(!laggingPrefix.apply(), "a page text that is not a prefix of the old page is refused");
        auto gone = planApply(plan, ScreenId{0x1000, 7}, true, "hello world", 2);
        check(!gone.apply() && std::string(applySkipReason(gone.skip)) == "page",
              "apply refused when the index is no longer a page");
        auto full = planApply(plan, ScreenId{0x1000, 7}, true, "hello world", 49);
        check(!full.apply() && std::string(applySkipReason(full.skip)) == "no-room",
              "apply refused when 50 pages would be exceeded");
        auto exactly = planApply(plan, ScreenId{0x1000, 7}, true, "hello world", 48);
        check(exactly.apply() && exactly.add == 2, "apply allowed when the pages land on exactly 50");
        DeferredPages none;
        none.head = "hello world";
        check(!planApply(none, ScreenId{}, true, "hello world", 1).apply(),
              "a plan with no kept pages is not applied");
        check(std::string(applySkipReason(ApplySkip::Empty)) == "empty", "an empty plan says so");
    }

    // A deferred plan (and its refusal) waits for a frame after the paste.
    {
        check(!planDue(12, 12), "a plan queued this frame does not apply in it");
        check(planDue(12, 13), "a plan applies on the next frame");
        check(!planDue(13, 12), "a plan never applies on an earlier frame");
    }

    // A paste that ends exactly on a page boundary adds no empty page: the
    // game showed a blank page between the pasted pages and the pages that
    // already followed (2026-10-27).
    {
        auto narrow = [](std::string const& text) { return text.size() <= 5; };
        auto split = splitPaste("", "", "aaaaabbbbb\n", 10, 5, narrow);
        check(split.kind == SplitKind::Pages && split.pages.size() == 2, "a paste ending at a page break adds no blank page");
        check(split.pages.size() == 2 && split.pages[1] == "bbbbb", "the last page holds the last characters without the break");
        // A pasted text that ends with line breaks must not leave a page holding
        // only them (the blank page seen in game 2026-10-27).
        auto trailing = splitPaste("", "", "aaaaabbbbb\n\n", 10, 5, narrow);
        check(trailing.kind == SplitKind::Pages && trailing.pages.size() == 2,
              "trailing line breaks add no page of their own");
        check(trailing.pages.size() == 2 && trailing.pages[1] == "bbbbb", "the trailing line breaks are dropped, not a page");
        // The text after the caret rides on the last planned page.
        auto tail = splitPaste("", "ABC", "aaaaabbbbb", 10, 5, narrow);
        check(tail.kind == SplitKind::Pages && tail.pages.size() == 3 && tail.pages.back() == "ABC",
              "the text after the caret follows the paste onto its own page when it does not fit");
    }

    // The page a paste belongs to, from the text the box holds: the spread's
    // idea of the current page can name the half the player just left.
    {
        using lamium::inventory::bookPaste::choosePastedPage;
        std::vector<std::string_view> two{"left page", "right page"};
        auto preferred = choosePastedPage("right page", 1, two);
        check(preferred.ok() && preferred.index == 1, "the page the spread names is used when its text matches");
        // Edited the right page, moved back to the left one: the spread still
        // names page 1, the box holds page 0's text (seen in game 2026-10-27).
        auto found = choosePastedPage("left page", 1, two);
        check(found.ok() && found.index == 0, "a page whose stored text matches is found when the spread names another");
        auto typed = choosePastedPage("left pageX", 1, two);
        check(typed.ok() && typed.index == 0 && std::string(typed.reason) == "pending-typing",
              "a page the live text extends is found the same way");
        std::vector<std::string_view> blanks{"", ""};
        auto empty = choosePastedPage("", 1, blanks);
        check(empty.ok() && empty.index == 1, "an empty box on an empty page stays with the spread's page");
        std::vector<std::string_view> growing{"ab", "abcd"};
        auto longest = choosePastedPage("abcdX", 0, growing);
        check(longest.ok() && longest.index == 1, "the longest matching neighbour wins over a shorter preferred page");
        check(!choosePastedPage("abcdX", -1, growing).ok(), "no preferred page selects no page");
        // An empty or freshly typed box matches every empty page; only the
        // spread around the preferred page may take the overflow.
        std::vector<std::string_view> book{"", "text", "", "", "more", "end"};
        auto near = choosePastedPage("", 4, book);
        check(near.ok() && near.index == 3, "an empty box finds the empty page beside the preferred one");
        check(!choosePastedPage("", 1, book).ok(), "two empty neighbours of the preferred page cannot be told apart");
        check(!choosePastedPage("", 4, std::vector<std::string_view>{"", "a", "b", "c", "more"}).ok(),
              "an empty page far from the spread is never chosen");
        auto exactFirst = choosePastedPage("left", 1, std::vector<std::string_view>{"left", ""});
        check(exactFirst.ok() && exactFirst.index == 0, "an exact neighbour wins over a preferred page that only prefixes");
        std::vector<std::string_view> unrelated{"one", "two"};
        check(!choosePastedPage("three", 0, unrelated).ok(), "a box holding another page's text selects no page");
        std::vector<std::string_view> photo{"", "\x01"};
        auto skipped = choosePastedPage("", 0, photo);
        check(skipped.ok() && skipped.index == 0, "a photo page is never taken for the text page beside it");
    }
}
