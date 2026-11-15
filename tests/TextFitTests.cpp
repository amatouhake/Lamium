#include "ui/TextFit.h"
#include <limits>
#include <utility>
void check(bool, char const*);
void textFitTests() {
    using lamium::ui::fitLabel;
    using lamium::ui::wrapLabel;
    auto measure = [](std::string_view text) {
        float width = 0;
        for (unsigned char c : text) if ((c & 0xc0) != 0x80) width += c < 128 ? 1 : 2;
        return width;
    };
    check(fitLabel("abc",3,measure) == "abc", "exact fitting labels remain unchanged");
    check(fitLabel("abcdefgh",6,measure) == "abc...", "long labels reserve ellipsis width");
    check(fitLabel("日本語設定",7,measure) == "日本...", "Japanese truncation preserves whole characters");
    check(fitLabel("A🌿日本語",6,measure) == "A🌿...", "four-byte characters survive truncation");
    check(fitLabel("abcdef",2,measure).empty(), "too little space for ellipsis draws no text");
    check(fitLabel("a",0,measure).empty() && fitLabel("a",std::numeric_limits<float>::quiet_NaN(),measure).empty(),
          "invalid label width draws no text");
    check(wrapLabel("one two three",7,3,measure) == std::vector<std::string>{"one two", "three"},
          "paragraphs wrap at word boundaries");
    check(wrapLabel("one three",6,3,measure) == std::vector<std::string>{"one", "three"},
          "words move intact to the following line");
    check(wrapLabel("日本語設定",4,3,measure) == std::vector<std::string>{"日本", "語設", "定"},
          "Japanese descriptions wrap at complete characters");
    check(wrapLabel("A🌿日本語",3,3,measure) == std::vector<std::string>{"A🌿", "日", "..."},
          "bounded paragraphs preserve emoji and truncate the final line");
    check(wrapLabel("abcdefghi",4,2,measure) == std::vector<std::string>{"abcd", "e..."},
          "unbroken text wraps and reserves final ellipsis");
    check(wrapLabel("one\ntwo\nthree",7,2,measure) == std::vector<std::string>{"one", "two ..."},
          "explicit line breaks cannot escape the final line budget");
    check(wrapLabel("日本",1,3,measure).empty() && wrapLabel("abc",4,0,measure).empty(),
          "unrenderable paragraphs terminate without partial characters");
    using lamium::ui::wrapFullText;
    check(wrapFullText("日本語設定",4,measure) == std::vector<std::string>{"日本", "語設", "定"}
        && wrapFullText("a\n\nb",4,measure) == std::vector<std::string>{"a", "", "b"}
        && wrapFullText("🌿日本",1,measure) == std::vector<std::string>{"🌿", "日", "本"},
        "full help preserves blank lines and every complete UTF-8 codepoint without ellipsis");
    using lamium::ui::RunKind;
    auto zero = [](std::string_view glyph) { return glyph == "件"; };
    auto runs = lamium::ui::labelRuns("10 件あり", zero);
    check(runs.size() == 3 && runs[0].text == "10 " && runs[0].kind == RunKind::Ascii && runs[1].text == "件"
              && runs[1].kind == RunKind::Zero && runs[2].text == "あり" && runs[2].kind == RunKind::Wide,
          "a zero-wide character is a run of its own between the ASCII and the other text");
    auto pair = lamium::ui::labelRuns("件件", zero);
    check(pair.size() == 2 && pair[0].text == "件" && pair[1].text == "件", "zero-wide characters never share a run");
    check(lamium::ui::labelRuns("", zero).empty() && lamium::ui::labelRuns("条目", zero).size() == 1,
          "text without such a character keeps one run per script");
    check(lamium::ui::zeroGlyphs("条件 3 件", zero) == 2 && lamium::ui::zeroGlyphs("abc", zero) == 0,
          "zero-wide characters are counted for the width they are owed");
}
