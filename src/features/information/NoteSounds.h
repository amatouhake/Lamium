#pragma once
#include <optional>
class BlockPos;
class Level;
namespace lamium::information {
// The note a note block last played, per cell (L-301). A block entity holds the
// note the world loaded with and never updates it, but every play sends
// LevelSoundEvent::Note with the note in the event's own data, so the sound is
// the live source - and it follows a tune by any player, not only this one.
std::optional<int> playedNote(BlockPos const& pos, int dimension);
// Subscribes to the level's sound events once; safe to call every frame.
void beginNoteSounds(Level& level, int dimension);
// Leaves the sound events and forgets the notes; safe to call every frame.
void endNoteSounds();
// Also forgets a failed subscription when the world closes.
void forgetNoteSounds();
}
