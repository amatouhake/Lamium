#include "ui/SearchQuery.h"
#include <string>
void check(bool, char const*);
void searchQueryTests() {
    lamium::ui::SearchQuery query;
    check(query.matches("anything"), "empty search includes all settings");
    check(query.append("  ZOOM  wheel "), "append search text");
    check(query.matches("camera.wheelStep zoom Wheel step"), "case-insensitive token matching");
    check(!query.matches("zoom Magnification"), "all search words must match");
    query.clear();
    check(query.append("空のシュルカー"), "Japanese input retained");
    check(query.matches("空のシュルカーを表示: {}"), "Japanese label search");
    check(query.backspace() && query.value() == "空のシュルカ", "backspace removes a UTF-8 codepoint");
    query.clear();
    check(!query.backspace(), "backspace empty search is safe");
    check(!query.append("\n") && !query.append("\b"), "control characters do not enter query");
    check(query.append(std::string(127, 'a')), "search length below limit");
    check(!query.append("あ") && query.value().size() == 127, "limit never splits UTF-8 event");
}
