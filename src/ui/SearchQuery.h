#pragma once
#include <string>
#include <string_view>

namespace lamium::ui {
// Native text events supply UTF-8. Keep codepoints intact when deleting and
// never truncate an event at an arbitrary byte boundary. ASCII case folding
// leaves Japanese and other UTF-8 text available for exact substring matching.
class SearchQuery {
    std::string text;
public:
    std::string const& value() const { return text; }
    void clear() { text.clear(); }
    bool append(std::string_view input) {
        if (input.empty() || text.size() + input.size() > 128) return false;
        for (unsigned char ch : input) if (ch < 32 || ch == 127) return false;
        text.append(input);
        return true;
    }
    bool backspace() {
        if (text.empty()) return false;
        auto start = text.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
        text.resize(start);
        return true;
    }
    static std::string folded(std::string_view input) {
        std::string result(input);
        for (auto& ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        return result;
    }
    bool matches(std::string_view haystack) const {
        auto source = folded(haystack);
        auto query = folded(text);
        size_t begin = 0;
        while (begin < query.size()) {
            begin = query.find_first_not_of(' ', begin);
            if (begin == std::string::npos) break;
            auto end = query.find(' ', begin);
            if (end == std::string::npos) end = query.size();
            if (source.find(query.substr(begin, end - begin)) == std::string::npos) return false;
            begin = end;
        }
        return true;
    }
};
}
