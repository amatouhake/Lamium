#include "ui/NumberInput.h"
#include "ui/SearchQuery.h"
#include <string>
void check(bool, char const*);
void textEditTests() {
    using namespace lamium::ui;
    SearchQuery field;
    field.append("abcd");
    field.left();
    field.left();
    field.append("X");
    check(field.value() == "abXcd" && field.caret() == 3, "typing inserts at the caret");
    check(field.backspace() && field.value() == "abcd" && field.caret() == 2, "backspace removes the character before the caret");
    check(field.erase() && field.value() == "abd" && field.caret() == 2, "delete removes the character after the caret");
    field.end();
    check(!field.erase() && field.value() == "abd", "delete at the end does nothing");
    field.home();
    check(!field.backspace() && field.caret() == 0, "backspace at the start does nothing");

    // Multi-byte text: the caret never stops inside a character.
    SearchQuery japanese;
    japanese.append("ガラス窓");
    japanese.left();
    check(japanese.caret() == std::string("ガラス").size(), "left steps over one whole character");
    japanese.append("の");
    check(japanese.value() == "ガラスの窓", "Japanese text is inserted between characters");
    japanese.place(4);
    check(japanese.caret() == 3, "a click inside a character lands on its start");
    check(japanese.backspace() && japanese.value() == "ラスの窓", "backspace removes a whole Japanese character");

    // Words.
    SearchQuery words;
    words.append("night vision key");
    words.left(true);
    check(words.caret() == std::string("night vision ").size(), "ctrl+left goes to the start of the word");
    words.left(true);
    check(words.caret() == std::string("night ").size(), "again: over the space to the previous word");
    words.right(true);
    check(words.caret() == std::string("night vision").size(), "ctrl+right goes to the end of the word");

    // Select-all.
    SearchQuery all;
    all.append("hello");
    all.selectAll();
    all.left();
    check(!all.selectedAll() && all.caret() == 0, "left ends a select-all at the start");
    all.selectAll();
    all.right();
    check(all.caret() == 5, "right ends it at the end");
    all.selectAll();
    check(all.erase() && all.value().empty(), "delete clears a select-all");

    // Paste.
    SearchQuery pasted;
    pasted.append("ab");
    pasted.home();
    check(pasted.paste("x\r\ny\tz") && pasted.value() == "x y zab" && pasted.caret() == 5, "pasted line breaks and tabs become spaces");
    SearchQuery full;
    full.append(std::string(126, 'a'));
    check(full.paste("bcd") && full.value().size() == 128, "a paste is cut to the limit");
    SearchQuery cut;
    cut.append(std::string(126, 'a'));
    check(cut.paste("ガ") == false && cut.value().size() == 126, "a character that does not fit is not split");

    // IME: a composition's backspaces act before a caret in the middle.
    SearchQuery ime;
    ime.append("AB");
    ime.left();
    ime.type("か");
    ime.type("\x08" "家");
    check(ime.value() == "A家B" && ime.caret() == std::string("A家").size(), "an IME rewrite replaces what it typed at the caret");

    // Numbers keep their digit rules with a caret.
    NumberInput number;
    number.begin(12.5f);
    check(number.selectedAll() && number.value() == "12.5", "a number starts selected");
    number.end();
    number.left();
    number.left();
    check(number.append("7") && number.value() == "127.5", "digits go in at the caret");
    check(!number.append(".") && number.value() == "127.5", "a second dot is refused");
    number.home();
    check(number.append("-") && number.value() == "-127.5", "minus only at the start");
    check(!number.append("-"), "and only once");
    check(!number.paste("abc") && number.value() == "-127.5", "pasted letters are refused");
    using lamium::ui::EditKey;
    using lamium::ui::editKey;
    check(editKey(0x08, false) == EditKey::Backspace && editKey(0x08, true) == EditKey::Backspace, "Backspace edits the typed name or number");
    check(editKey(0x41, true) == EditKey::SelectAll && editKey(0x41, false) == EditKey::None, "A selects all only with Ctrl");
    check(editKey(0x1b, false) == EditKey::Cancel && editKey(0x0d, false) == EditKey::Finish && editKey(0x09, true) == EditKey::Finish,
          "Esc drops the edit, Enter and Tab keep it");
    check(editKey(0x31, false) == EditKey::None && editKey(0x26, false) == EditKey::None,
          "digits and arrows are not the editor's keys: text comes through the native keyboard");
}
