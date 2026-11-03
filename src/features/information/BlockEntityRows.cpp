#include "features/information/BlockEntityRows.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace lamium::information {
namespace {
using schematic::nbt::Compound;
using schematic::nbt::Tag;

// A backstop only: the card grows to the width it needs and its own text fitting
// decides what is visible, so a value is cut here only when it is long enough to
// make measuring it pointless. The stored text stays complete in the block entity.
constexpr std::size_t valueLimit = 256;

std::string shortened(std::string text) {
    if (text.size() <= valueLimit) return text;
    // Cut on a character boundary so a multi-byte character is never split.
    std::size_t cut = valueLimit;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
    text.resize(cut);
    text += "…";
    return text;
}
// A stored text keeps its own line breaks in one string: the card has one row
// for it, so the breaks read as a separator.
std::string lines(std::string const& text) {
    std::string out;
    out.reserve(text.size());
    for (char character : text) {
        if (character == '\n') out += " / ";
        else out += character;
    }
    return out;
}
std::string join(std::vector<std::string> const& parts) {
    std::string out;
    for (auto const& part : parts) {
        if (!out.empty()) out += ", ";
        out += part;
    }
    return out;
}
// An item stack stores the item's id; the player reads its name. `Count` is 1
// for most stacks and the game writes it as a byte or a short.
std::optional<std::string> itemName(Tag const& tag, BlockEntityNames const& names) {
    auto const* compound = tag.as<Compound>();
    if (!compound) return std::nullopt;
    auto const* name = compound->find("Name");
    auto const* id = name ? name->as<std::string>() : nullptr;
    if (!id) return std::nullopt;
    std::string text = names.item ? names.item(*id) : std::string{};
    if (text.empty()) text = *id;
    std::int64_t count = 1;
    if (auto const* stored = compound->find("Count")) stored->integer(count);
    if (count > 1) text += std::format(" ×{}", count);
    return text;
}
// A block entry (a flower pot's plant) stores the block's id under `name`,
// lowercase, unlike an item stack's `Name`.
std::optional<std::string> blockName(Tag const& tag, BlockEntityNames const& names) {
    auto const* compound = tag.as<Compound>();
    if (!compound) return std::nullopt;
    auto const* name = compound->find("name");
    auto const* id = name ? name->as<std::string>() : nullptr;
    if (!id) return std::nullopt;
    std::string text = names.block ? names.block(*id) : std::string{};
    return text.empty() ? std::optional<std::string>(*id) : std::optional<std::string>(std::move(text));
}
// A list of ids (a pot's sherds) or of item stacks (a shelf's books). A mixed
// list keeps the game's own notation rather than guessing.
std::optional<std::string> listText(Tag const& tag, BlockEntityNames const& names) {
    auto const* list = tag.as<schematic::nbt::List>();
    if (!list || list->items.empty()) return std::nullopt;
    std::vector<std::string> parts;
    for (auto const& item : list->items) {
        if (auto const* text = item.as<std::string>()) parts.push_back(*text);
        else if (auto name = itemName(item, names)) parts.push_back(*name);
        else return std::nullopt;
    }
    return join(parts);
}
// A key whose value reads as yes/no: the sign's and the lectern's own flags.
bool flagField(std::string_view key) {
    for (auto prefix : {std::string_view{"Is"}, std::string_view{"has"}, std::string_view{"Hide"},
                        std::string_view{"Ignore"}})
        if (key.starts_with(prefix)) return true;
    return false;
}
}

std::string notePitch(long long note) {
    // Note 0 plays F#3 and each step a semitone up; 24 is F#5.
    constexpr std::string_view names[] = {"F#3", "G3",  "G#3", "A3",  "A#3", "B3",  "C4",  "C#4", "D4",
                                          "D#4", "E4",  "F4",  "F#4", "G4",  "G#4", "A4",  "A#4", "B4",
                                          "C5",  "C#5", "D5",  "D#5", "E5",  "F5",  "F#5"};
    if (note < 0 || note >= static_cast<long long>(std::size(names))) return {};
    return std::string(names[note]);
}

bool blockEntityFieldHidden(std::string_view key) {
    // `id` names the actor type (the card shows the block), `x`/`y`/`z` its
    // position (the card shows it on request), and the two flags are written
    // on every actor without saying anything about this block.
    for (auto hidden : {std::string_view{"id"}, std::string_view{"x"}, std::string_view{"y"},
                        std::string_view{"z"}, std::string_view{"isMovable"}, std::string_view{"Findable"}})
        if (key == hidden) return true;
    return false;
}

std::string blockEntityValue(std::string_view key, Tag const& tag, BlockEntityNames const& names) {
    if (auto const* text = tag.as<std::string>()) return shortened(lines(*text));
    std::int64_t number = 0;
    if (tag.integer(number)) {
        // L-300: the note block's pitch lives here, not in its states. The card
        // passes `note` as stale until L-301 keeps it current, so this reading
        // is for that item.
        if (key == "note" && number >= 0 && number <= 24)
            return std::format("{} / 24 ({})", number, notePitch(number));
        // A sign's text colour is a 32-bit ARGB value; the player reads #RRGGBB.
        if (key.ends_with("Color") || key.ends_with("Colour"))
            return std::format("#{:06X}", static_cast<std::uint32_t>(number) & 0xFFFFFFu);
        return std::to_string(number);
    }
    switch (tag.type()) {
    case schematic::nbt::Type::Float: return std::format("{}", *tag.as<float>());
    case schematic::nbt::Type::Double: return std::format("{}", *tag.as<double>());
    case schematic::nbt::Type::List:
        if (auto text = listText(tag, names)) return shortened(*text);
        if (tag.as<schematic::nbt::List>()->items.empty()) return {};
        return shortened(schematic::nbt::text(tag));
    case schematic::nbt::Type::Compound:
        if (tag.as<Compound>()->entries.empty()) return {};
        if (auto const* text = tag.as<Compound>()->find("Text"); text && text->as<std::string>())
            return shortened(lines(*text->as<std::string>()));
        if (auto name = itemName(tag, names)) return shortened(*name);
        if (auto name = blockName(tag, names)) return shortened(*name);
        return shortened(schematic::nbt::text(tag));
    default: return shortened(schematic::nbt::text(tag));
    }
}

namespace {
// An item stack and a block entry read as one name; any other compound keeps
// its own fields, so a sign's colour and glow become rows instead of hiding
// inside the text compound that carried them.
bool storesFields(Tag const& tag) {
    auto const* compound = tag.as<Compound>();
    if (!compound || compound->entries.empty()) return false;
    return !compound->find("Name") && !compound->find("name");
}
bool listed(std::string_view key, std::span<std::string_view const> stale) {
    return std::find(stale.begin(), stale.end(), key) != stale.end();
}
void appendField(std::vector<TargetInfo::DetailRow>& rows, std::string const& key, Tag const& tag,
                 BlockEntityNames const& names, std::span<std::string_view const> stale, std::optional<int> note,
                 int depth = 0) {
    if (blockEntityFieldHidden(key) || listed(key, stale)) return;
    std::string value = blockEntityValue(key, tag, names);
    // L-301: the block only stores the note the world loaded with; a sound the
    // card has heard since is the current pitch.
    if (key == "note" && note) value = std::format("{} / 24 ({})", *note, notePitch(*note));
    // Nothing stored: an empty string, an empty compound, an empty list.
    if (!value.empty()) {
        TargetInfo::DetailRow row;
        // Plain text either way: the label is already translated, or the key itself.
        std::string label = names.label ? names.label(key) : std::string{};
        row.label = label.empty() ? key : std::move(label);
        row.labelIsKey = false;
        // The game stores a flag as a byte; a wider integer is a count or an id.
        std::int64_t flag = 0;
        if (flagField(key) && tag.type() == schematic::nbt::Type::Byte && tag.integer(flag)
            && (flag == 0 || flag == 1)) {
            row.value = flag ? "target.yes" : "target.no";
            row.valueIsKey = true;
        } else row.value = std::move(value);
        row.kind = DetailKind::Other;
        rows.push_back(std::move(row));
    }
    if (depth >= 1 || !storesFields(tag)) return;
    for (auto const& child : tag.as<Compound>()->entries) {
        // `Text` is this field's own value, already shown by the row above.
        if (child.name == "Text") continue;
        appendField(rows, child.name, child.tag, names, stale, note, depth + 1);
    }
}
}

std::vector<TargetInfo::DetailRow> blockEntityRows(Compound const& data, BlockEntityNames const& names,
                                                   std::span<std::string_view const> stale, std::optional<int> note) {
    std::vector<TargetInfo::DetailRow> rows;
    for (auto const& entry : data.entries) appendField(rows, entry.name, entry.tag, names, stale, note);
    return rows;
}
}
