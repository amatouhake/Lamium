#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace lamium::ui {
// What a key does while a name or a number is being typed in a view (DESIGN
// "Controls"): Backspace edits, Ctrl + A selects it all, Esc drops the edit,
// Enter and Tab keep it. The text itself comes through the native keyboard.
enum class EditKey { None, Backspace, SelectAll, Cancel, Finish };
constexpr EditKey editKey(int key, bool ctrl) {
    switch (key) {
    case 0x08: return EditKey::Backspace;
    case 0x41: return ctrl ? EditKey::SelectAll : EditKey::None;
    case 0x1b: return EditKey::Cancel;
    case 0x0d: case 0x09: return EditKey::Finish;
    default: return EditKey::None;
    }
}
// The editing core of every Lamium text field (L-172): UTF-8 text with a caret
// kept on codepoint boundaries, select-all as the only selection, and the
// edits the fields share. Fields add their own character rules (`accepts`).
class TextEdit {
protected:
    std::string text;
    std::size_t at = 0; // caret, a byte offset on a codepoint boundary
    bool replace = false;
    std::size_t limit;
    // Whether `next` may become the field's text.
    virtual bool accepts(std::string const& next) const { return next.size() <= limit; }
    static bool continuation(char ch) { return (static_cast<unsigned char>(ch) & 0xc0) == 0x80; }
    std::size_t previous(std::size_t from) const {
        if (from == 0) return 0;
        --from;
        while (from > 0 && continuation(text[from])) --from;
        return from;
    }
    std::size_t next(std::size_t from) const {
        if (from >= text.size()) return text.size();
        ++from;
        while (from < text.size() && continuation(text[from])) ++from;
        return from;
    }
public:
    explicit TextEdit(std::size_t limit, bool replace = false) : replace(replace), limit(limit) {}
    virtual ~TextEdit() = default;
    std::string const& value() const { return text; }
    std::size_t caret() const { return at; }
    bool selectedAll() const { return replace; }
    void selectAll() { replace = true; }
    void clear() { text.clear(); at = 0; replace = false; }
    // Typed or pasted text at the caret (replacing everything when selected);
    // control characters refuse the whole input.
    bool append(std::string_view input) {
        if (input.empty()) return false;
        for (unsigned char ch : input) if (ch < 32 || ch == 127) return false;
        std::string after = replace ? std::string(input) : text.substr(0, at) + std::string(input) + text.substr(at);
        if (!accepts(after)) return false;
        at = (replace ? 0 : at) + input.size();
        text = std::move(after);
        replace = false;
        return true;
    }
    // A native text event as the game sends it. An IME rewriting its
    // composition sends a backspace (0x08) for each character it takes back
    // before the new ones; other control characters are dropped.
    bool type(std::string_view input) {
        bool changed = false;
        std::size_t start = 0;
        auto flush = [&](std::size_t end) {
            if (end > start) changed = append(input.substr(start, end - start)) || changed;
        };
        for (std::size_t i = 0; i < input.size(); ++i) {
            auto ch = static_cast<unsigned char>(input[i]);
            if (ch >= 32 && ch != 127) continue;
            flush(i);
            if (ch == 8) changed = backspace() || changed;
            start = i + 1;
        }
        flush(input.size());
        return changed;
    }
    // Clipboard text: line breaks and tabs become spaces, other control
    // characters go, and what does not fit the limit is cut on a codepoint.
    bool paste(std::string_view input) {
        std::string clean;
        for (char ch : input) {
            auto byte = static_cast<unsigned char>(ch);
            if (ch == '\r') continue;
            if (ch == '\n' || ch == '\t') clean += ' ';
            else if (byte >= 32 && byte != 127) clean += ch;
        }
        while (!clean.empty()) {
            if (append(clean)) return true;
            auto cut = clean.size() - 1;
            while (cut > 0 && continuation(clean[cut])) --cut;
            clean.resize(cut);
        }
        return false;
    }
    bool backspace() {
        if (text.empty()) return false;
        if (replace) { clear(); return true; }
        if (at == 0) return false;
        auto from = previous(at);
        text.erase(from, at - from);
        at = from;
        return true;
    }
    // The Delete key: the character after the caret.
    bool erase() {
        if (text.empty()) return false;
        if (replace) { clear(); return true; }
        if (at >= text.size()) return false;
        text.erase(at, next(at) - at);
        return true;
    }
    // Moving ends a select-all at the side moved toward. By word: over spaces,
    // then over the word.
    void left(bool word = false) {
        if (std::exchange(replace, false)) { at = 0; return; }
        if (!word) { at = previous(at); return; }
        while (at > 0 && text[previous(at)] == ' ') at = previous(at);
        while (at > 0 && text[previous(at)] != ' ') at = previous(at);
    }
    void right(bool word = false) {
        if (std::exchange(replace, false)) { at = text.size(); return; }
        if (!word) { at = next(at); return; }
        while (at < text.size() && text[at] == ' ') at = next(at);
        while (at < text.size() && text[at] != ' ') at = next(at);
    }
    void home() { replace = false; at = 0; }
    void end() { replace = false; at = text.size(); }
    // Removes the text from `from` up to the caret (a select-all clears).
    bool eraseBefore(std::size_t from) {
        if (replace) { bool had = !text.empty(); clear(); return had; }
        if (from >= at) return false;
        text.erase(from, at - from);
        at = from;
        return true;
    }
    // A click: the caret goes to `byte`, moved back onto a codepoint boundary.
    void place(std::size_t byte) {
        replace = false;
        at = byte > text.size() ? text.size() : byte;
        while (at > 0 && at < text.size() && continuation(text[at])) --at;
    }
    // Replaces the text and puts the caret at its end.
    void set(std::string value) {
        text = std::move(value);
        at = text.size();
        replace = false;
    }
};
}
