#pragma once
#include <string_view>

namespace lamium::inventory::bookPaste {
// L-259 trace build only (`--book_paste_trace=y`): read-only hooks that record
// which path a paste into a book and quill takes. A release build starts and
// stops nothing.
void startBookPaste();
void stopBookPaste();

// UTF-8 sequences, which is what the game counts as characters; bytes are the
// other unit L-259 has to tell apart (Japanese is three bytes per character).
constexpr int codePointCount(std::string_view text) {
    int count = 0;
    for (unsigned char c : text) if ((c & 0xc0) != 0x80) ++count;
    return count;
}
}
