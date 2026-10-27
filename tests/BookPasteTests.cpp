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
}
