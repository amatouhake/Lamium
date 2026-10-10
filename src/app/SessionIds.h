#pragma once
#include <cstdint>
#include <set>
#include <vector>

// Session ids (BACKLOG L-139): each entry of a list (a waypoint, a schematic
// placement) gets an id when it is loaded or added, never reused within the
// session and kept through reordering, edits and other entries' deletion, so
// a selection made by id cannot move to another entry. Not saved: selections
// end with the world anyway. Entries carry the id in a member `id`; 0 means
// none yet.
namespace lamium {
// Gives entries without an id, and every later copy of a repeated id, a new
// one from `next`.
template <class Entry>
void assignSessionIds(std::vector<Entry>& entries, std::uint64_t& next) {
    std::set<std::uint64_t> seen;
    for (auto& entry : entries) {
        if (entry.id && seen.insert(entry.id).second) continue;
        entry.id = next++;
        seen.insert(entry.id);
    }
}
// The entry's position now, or -1 when it is gone.
template <class Entry>
int indexOfId(std::vector<Entry> const& entries, std::uint64_t id) {
    if (!id) return -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return static_cast<int>(i);
    return -1;
}
}
