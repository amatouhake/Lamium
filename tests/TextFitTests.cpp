#include "ui/TextFit.h"
#include <limits>
void check(bool, char const*);
void textFitTests() {
    using lamium::ui::fitLabel;
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
}
