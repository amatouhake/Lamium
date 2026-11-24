#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// The Files tab's rows (L-93 screen review): files grouped under a heading
// per folder, files at the top of the folder first with no heading unless
// subfolders exist too. Pure; `relative` paths use forward slashes.
namespace lamium::ui::schematic_files {
struct Row {
    int file = -1;       // index into the file list; -1 for a folder heading
    std::string folder;  // the heading's folder ("farms/"), empty for the top
};
inline std::string_view folderOf(std::string_view relative) {
    auto slash = relative.find_last_of('/');
    return slash == std::string_view::npos ? std::string_view{} : relative.substr(0, slash + 1);
}
inline std::vector<Row> rows(std::vector<std::string> const& relatives) {
    std::vector<int> order(relatives.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    // Top-level files first, then folders by name; inside, by path.
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        auto fa = folderOf(relatives[static_cast<size_t>(a)]), fb = folderOf(relatives[static_cast<size_t>(b)]);
        if (fa.empty() != fb.empty()) return fa.empty();
        if (fa != fb) return fa < fb;
        return relatives[static_cast<size_t>(a)] < relatives[static_cast<size_t>(b)];
    });
    bool folders = false;
    for (auto const& r : relatives) folders = folders || !folderOf(r).empty();
    std::vector<Row> out;
    std::string_view current;
    bool first = true;
    for (int i : order) {
        auto folder = folderOf(relatives[static_cast<size_t>(i)]);
        if (folders && (first || folder != current)) out.push_back({-1, std::string(folder)});
        current = folder;
        first = false;
        out.push_back({i, {}});
    }
    return out;
}
// The row index showing a file, or -1.
inline int rowOf(std::vector<Row> const& rows, int file) {
    for (size_t r = 0; r < rows.size(); ++r) if (rows[r].file == file) return static_cast<int>(r);
    return -1;
}
// The file one step down (`step` 1) or up (-1) the rows from `file`, past
// folder headings; from no file (-1), the first going down and the last going
// up. -1 when there is none that way.
inline int stepFile(std::vector<Row> const& rows, int file, int step) {
    int count = static_cast<int>(rows.size());
    int at = file >= 0 ? rowOf(rows, file) : (step > 0 ? -1 : count);
    for (int r = at + step; r >= 0 && r < count; r += step)
        if (rows[static_cast<size_t>(r)].file >= 0) return rows[static_cast<size_t>(r)].file;
    return -1;
}
}
