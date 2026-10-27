#include "features/inventory/BookPaste.h"
#include <string>
void check(bool, char const*);
void bookPasteTests() {
    using lamium::inventory::bookPaste::codePointCount;
    check(codePointCount("") == 0, "empty text has no code points");
    check(codePointCount("book") == 4, "ASCII counts one code point per byte");
    check(std::string("本の栞").size() == 9 && codePointCount("本の栞") == 3,
          "three Japanese characters are nine bytes but three code points");
    check(codePointCount("A🌿日本") == 4, "a four-byte character counts once");
    check(codePointCount("\xe6\x9c\xac") == 1, "one three-byte sequence counts once");
    check(codePointCount("\xe6\x9c") == 1, "a truncated sequence still counts the bytes it has");
}
