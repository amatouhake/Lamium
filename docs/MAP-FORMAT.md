# Map region file format (LMR2)

Current contract, split out of MAP.md on 2026-11-14; earlier records: `git show 10d58d01:docs/MAP.md`.
How the recorded map is stored: [MAP.md](MAP.md) "Recording and storage".

## Region file format LMR2 (L-216, 2026-10-19)

Agreed with the maintainer. `LMR2` is written; `LMR1` (`.lmr`) stays
readable. Stored: colors seen and heights. Not blocks or biomes, so a pack
change does not recolor the map and game updates need no block-state
conversion (optional sections if ever wanted). About 22.7 KB per region
against 136 KB in `LMR1`; zlib 6 was slightly smaller at ~8x the CPU, hence
zstd 3.

```
file r.<x>.<z>.lm2 (256x256 blocks), little-endian
header (18 bytes): "LMR2" | u16 version (1) | u16 flags (0) | i32 region x | i32 region z | u16 section count
table (18 bytes per section):
  4-byte tag | u8 codec (0 none, 1 zstd) | u8 flags (bit 0 required, bit 1 keep)
  u32 raw size | u32 stored size | u32 CRC-32 of the stored bytes
bodies: in table order, right after the table, no padding, nothing after the last
```

The magic never changes; later formats raise the version. CRC-32 is
IEEE/zlib (`"123456789"` gives `0xCBF43926`). A zstd body is exactly one
frame, no dictionary, nothing after it; a frame content size, if present,
equals the raw size; stored size above 0. Codec 0: raw size = stored size.
One zstd context per worker. `required`: a reader that does not know the
tag or codec must not rewrite the file (it says nothing about which known
sections must be present). Tags are compared as 4 raw bytes; Lamium writes
only A-Z and 0-9, but readers never reject a tag for its bytes.

Sections in version 1 (zstd level 3, no setting):
- `CHNK` (required; may be absent): raw 2,560 bytes, 256 entries of 10 bytes
  in chunk order `z * 16 + x`: u64 recorded time (Unix ms, 0 = never), u16
  column revision (0 = unknown).
- `COLR` (required; must be present): u32 palette size `p` (1 to 65,536),
  `p` u32 colors in `packColor` order (r lowest byte, alpha highest), then
  one index per column row-major (z rows of x): u8 when `p <= 256`, else u16
  as a low-byte plane then a high-byte plane. Raw size exactly
  `4 + 4p + 65,536 * (p <= 256 ? 1 : 2)`. Indices below `p`. Alpha is 255,
  or the color is exactly 0 ("not recorded", an ordinary palette value).
- `HGHT` (required; must be present): raw 131,072 bytes. Each height as a
  difference from `prev = x > 0 ? h[z][x-1] : (z > 0 ? h[z-1][0] : 0)` (a row
  start refers to the north column) in 16-bit wrapping arithmetic:
  `u = uint16(h) - uint16(prev)`, stored
  `z = uint16((u << 1) ^ (u & 0x8000 ? 0xFFFF : 0))`; decoding
  `u = (z >> 1) ^ (z & 1 ? 0xFFFF : 0)`, `h = int16(uint16(prev) + u)`.
  Lossless over all i16, not limited to a world range. Low-byte plane, then
  high-byte plane. Unrecorded columns have height 0.
- `OLDF` (optional, not kept): raw 20 bytes for the absorbed `.lmr`: u64
  size, u64 modification time (`last_write_time` to the system clock,
  truncated to Unix ms), u32 CRC-32 of the whole file. Written with codec 0;
  both accepted.

### Reading

Loaded, Missing (no file), Damaged or Refused, checked in this order so a
future file is never taken for damaged. Size limits come from the table
before any body is allocated; unknown sections are never decompressed.
1. File above 8 MiB: Refused.
2. Under 4 bytes or magic not `LMR2`: Refused if it starts with `LMR`, else
   Damaged.
3. Version read once 6 bytes exist, before the header length check: under 6
   bytes or version 0: Damaged; above 1: Refused.
4. Header under 18 bytes: Damaged; header flags not 0: Refused.
5. Table: over 64 sections, a kept unknown section above 1 MiB stored or kept
   unknown ones above 2 MiB together, flag bits 2-7, or an unknown tag or
   codec with required set: Refused. A table that does not fit, bodies not
   adding up to the file size exactly, a known tag twice, or a known section
   of the wrong raw size (`COLR` at most 393,220 before its palette size is
   read): Damaged. Unknown tags may repeat.
6. Region x/z not matching the file name: Damaged.
7. Known section with a CRC mismatch, a body not decoding to exactly the raw
   size, an index out of range or another alpha: that section is missing.
   Without `COLR` or `HGHT`: Damaged. Without `CHNK`: all chunks time 0,
   revision 0.
8. Unknown optional sections: kept with keep (stored bytes as they are) if
   the CRC matches and `CHNK` was read; otherwise dropped.

Refused: logged once, not shown, never written; its `.lmr` neither read nor
deleted; what is recorded there stays in memory until far-resident eviction;
clearing the world's map still removes it. Damaged: logged; read from the
`.lmr` if any; the next save replaces the `.lm2` and deletes the `.lmr` as
below. Loaded with no `.lmr`: used as is.

### Unknown sections on a resave

With keep: written back unchanged (codec, flags, stored bytes, CRC); without:
dropped. A future column or chunk section sets keep only if it carries its
own per-chunk times; a newer reader treats a chunk whose `CHNK` time differs
from the section's as missing for that section. No `CHNK`: all kept sections
dropped. If kept plus known sections exceed 64 entries, saving fails with a
section-limit message before compression; nothing is dropped to make room
and existing files stay.

### Column revision

`columnRevision` (`MapColors.h`) is raised whenever a change makes earlier
colors or heights wrong (a tint bug, another surface block choice, another
cave window). Per chunk, since a region mixes builds. A chunk below the
build's revision shows as saved and is due for a rescan whenever loaded;
never discarded unseen.

`record` takes "produced" (final value is this scan's own: not for columns
the scan did not know, `fillFromSaved` filled, or with the stand-in tint) and
"scanned" (produced plus provisional); `underlay` sets neither. "Full": every
column the chunk has recorded after the merge is produced.

| Scan result | Revision | Time |
|---|---|---|
| full, colors or heights changed | the build's | now |
| full, nothing changed | max(stored, build's) | unchanged |
| partial, filled from saved, or provisional | min(stored, build's); a chunk new to the map 0 | now if a scanned column changed |
| no column scanned | unchanged | unchanged |

A revision-only change marks the region for saving without staling its
images. `scanChunk`'s unchanged-tile shortcut still calls `record` when the
stored revision is below the build's. When `underlay` fills columns under
scanned ones, a chunk that took disk columns gets the min revision and max
time of the two. The store reports chunks below the build's revision per
loaded region; `ScanGrid` treats them as partially seen. Surface and cave
scans share provisional-tint marking at `blockColor`; a cave scan that used
stand-ins waits for its saved region to merge before recording.

### Migration and older releases

- Join listing, settings usage and clearing include `.lm2` and `.lmr`.
  Reading prefers `.lm2`; when missing or Damaged, the `.lmr` is read by the
  `LMR1` decoder (all time 0, revision 0) and marked for saving once.
- An unreadable `.lmr` not matched by `OLDF` is retained and logged
  separately; it does not mark a valid `.lm2` damaged, schedule a save or get
  deleted as absorbed.
- Idle conversion (maintainer: convert the whole map in spare time): with no
  save, load, listing or image build due, the worker takes one non-resident
  region that had an `.lmr` in the join listing, loads the files as they are
  now exactly as a resident load would, and saves. Resident or Refused
  regions are skipped; no `.lmr` left means done. At most one per 100 ms,
  dropped on world or generation change. Until then, images of non-resident
  regions use the same load, merged in memory unsaved.
- Saving writes `.lm2` first (atomic replace) with the absorbed `.lmr` in
  `OLDF`; right before deleting the `.lmr` its size, time and CRC are checked
  again, and it is deleted only if they match (else the next load merges it).
- A region stays marked until its write succeeds or fails permanently, is not
  requeued while in flight, and stays marked if changed meanwhile. Every
  completion starts a fresh 10 s save delay. I/O failures retry after 10 s,
  20 s, 40 s ..., up to 5 minutes; success resets. Section-limit errors,
  Refused files and invalid region data are permanent: logged once per region
  per session, no retry or requeue (also after eviction), files untouched,
  not blocking eviction. World exit flushes pending changes once.
- An older Lamium does not list `.lm2` and records anew into `.lmr`. With a
  Loaded `.lm2` and an `.lmr`: if the `.lmr` matches `OLDF` it was left by an
  interrupted save and is deleted unmerged. Otherwise fill only: columns the
  `.lm2` lacks come from the `.lmr`, recorded ones are never overwritten; a
  chunk that took columns, or has a column recorded in both with a different
  color or height, gets revision 0 and the later time; then the region is
  saved with that `.lmr` in `OLDF`, which deletes it. File times and the
  clock never decide which colors win.
