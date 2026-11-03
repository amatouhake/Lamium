#pragma once
#include "features/information/TargetInfo.h"
#include "features/schematic/Nbt.h"
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class BlockPos;
class BlockSource;
namespace lamium::information {
// Block entity data as target-card rows (L-300). The card reads a block's
// states, and a block entity's own fields - a sign's text, a note block's
// pitch, a jukebox's record - are stored outside them, so no row ever showed
// them. Every saved field becomes a row: the card does not decide that a
// field is unimportant. Pure: the caller supplies the names a row may need,
// so all of this is table-tested without the game.
struct BlockEntityNames {
    // An item id as the player reads it; empty keeps the id as it stands.
    std::function<std::string(std::string_view id)> item;
    // A block id as the player reads it; empty keeps the id as it stands.
    std::function<std::string(std::string_view id)> block;
    // A field's label; empty keeps the field's own key as it stands.
    std::function<std::string(std::string_view key)> label;
};
// Fields that make no row: the block's own id and position (the card shows
// the name and, on request, the position) and the two flags the game writes
// on every actor. Everything else is shown.
bool blockEntityFieldHidden(std::string_view key);
// One field as text: an item stack reads as the item's name, a text compound
// as its lines, a list as its items, a note block's `note` as the pitch it
// plays. Anything else keeps the game's own notation, never dropped.
std::string blockEntityValue(std::string_view key, schematic::nbt::Tag const& tag,
                             BlockEntityNames const& names);
// Every stored field as one row, in the order the game saved them. A compound
// that stores fields of its own (a sign's text, with its colour and glow)
// keeps them as rows too, so nothing hides inside a field that also has text.
// A field that stores nothing (an empty string, an empty compound, an empty
// list) makes no row.
// `stale`: fields to leave out because the client never refreshes them. It
// keeps current only what it has to draw; a container's contents and a note
// block's pitch are the copy it got when the world loaded, so a row would read
// as the value now (L-300).
// `note`: a note block's pitch as the sound events last reported it, which
// replaces the stored one when the card has heard the block play (L-301).
std::vector<TargetInfo::DetailRow> blockEntityRows(schematic::nbt::Compound const& data,
                                                   BlockEntityNames const& names,
                                                   std::span<std::string_view const> stale = {},
                                                   std::optional<int> note = std::nullopt);
// A note block's pitch: note 0 plays F#3, 12 F#4, 24 F#5. Out of range is
// the note's own number, since the game would not play it.
std::string notePitch(long long note);

// With the game (TargetInfo.cpp): the block entity at `pos` as the client
// would save it, nothing when there is none or it will not read; and the
// names the card uses, shared by the schematic check (L-303).
std::optional<schematic::nbt::Compound> savedBlockEntity(BlockSource& source, BlockPos const& pos);
BlockEntityNames const& gameBlockEntityNames();
}
