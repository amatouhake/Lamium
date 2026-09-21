#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace lamium::inspection::preview {

// Pure, game-independent Bundle helpers: item-family recognition, content
// fingerprinting and dynamic-grid layout. Deliberately free of game types so
// the unit tests exercise the production functions directly.

/// True when `typeName` is a Bundle of any colour: exactly "minecraft:bundle"
/// or "<namespace>:<colour>_bundle" ("undyed_bundle" included). A plain
/// `ends_with("bundle")` would also match unrelated future items, so the '_'
/// (or ':') boundary before "bundle" is required.
[[nodiscard]] inline bool isBundleTypeName(std::string const& typeName) {
    constexpr std::string_view kSuffix = "bundle";
    if (!typeName.ends_with(kSuffix)) {
        return false;
    }
    if (typeName.size() == kSuffix.size()) {
        return true;
    }
    char const boundary = typeName[typeName.size() - kSuffix.size() - 1];
    return boundary == '_' || boundary == ':';
}

/// Bounded dynamic grid for the packed Bundle entries.
///
/// Shulker Boxes mirror the real container (fixed 9x3 with gaps); Bundles
/// store a packed list instead, so the grid is derived from the filled count:
/// rows grow only as needed, columns widen for large Bundles (8x8 at 64
/// entries still fits an ordinary full-HD inventory screen at vanilla GUI
/// sizes), and the heuristic prefers wider shapes (fewer rows) so the preview
/// stays short.
struct BundleGrid {
    /// Maximum preview slots. Bedrock storage items support up to 64 dynamic
    /// slots (`max_slots: 64` in the vanilla Bundle definition); every real
    /// non-empty entry is retained, never discarded.
    static constexpr int kMaxSlots = 64;
    /// Narrowest grid: keeps single entries readable instead of a 1-wide strip.
    static constexpr int kMinColumns = 3;
    /// Widest grid: 8 columns of 18-unit cells plus padding is 152 GUI units.
    /// Ordinary full-HD inventory screens are ~300+ units wide, so even a
    /// full 8x8 grid fits; `PreviewLayout::anchored` flips/clamps at edges.
    static constexpr int kMaxColumns = 8;
    /// Empty Bundle frame (drawn only when `bundle.showEmpty` is set): the
    /// narrowest grid, one row, so an empty Bundle is unobtrusive.
    static constexpr int kEmptyColumns = 3;
    static constexpr int kEmptyRows    = 1;

    int columns{0};
    int rows{0};

    /// Smallest grid holding `filled` entries within the column band, wider
    /// shapes preferred: columns grow before rows do. `filled <= 0` yields an
    /// empty (0x0) shape; the caller decides whether to draw it.
    [[nodiscard]] static constexpr BundleGrid shapeFor(int filled) {
        if (filled <= 0) {
            return BundleGrid{0, 0};
        }
        if (filled > kMaxSlots) {
            filled = kMaxSlots;
        }
        for (int columns = kMinColumns; columns <= kMaxColumns; ++columns) {
            if (filled <= columns * columns) {
                int const rows = (filled + columns - 1) / columns;
                return BundleGrid{columns, rows};
            }
        }
        // More entries than an 8x8 square: keep 8 columns and add rows.
        int const rows = (filled + kMaxColumns - 1) / kMaxColumns;
        return BundleGrid{kMaxColumns, rows};
    }
};

/// Lightweight content fingerprint for Bundle cache invalidation.
///
/// The hover cache re-keys every frame, so the fingerprint MUST NEVER touch
/// the item registry: `ItemStack::fromTag` resolves each entry by name and
/// rebuilds a full stack (up to 64 registry lookups per frame while
/// hovering). Each entry therefore contributes only its raw NBT identity —
/// the stored `Slot` index read straight off the entry map, the entry's tag
/// `Type` id (so a Compound<->non-Compound flip changes the key) and the
/// entry's `Tag::hash()` (covers Name/Count/Damage/tag/... without
/// decoding). Chained in list order with FNV-1a: order-sensitive,
/// allocation-free, and game-independent so the unit tests pin the
/// production mixers directly.
///
/// `fingerprintBundleFinal` folds the Bundle stack's own id/aux/count plus
/// the entry count, so a swapped-in Bundle with identical contents still
/// re-extracts, and a grown/shrunk list changes the key even if the
/// surviving entries hash the same. An empty Bundle (no `Items` list) hashes
/// the tail over a zero seed: stable across identical empties.

/// Slot contribution for entries that carry no usable slot: null pointers
/// and non-Compound elements. Real slots are >= -1 (`Slot` absent yields
/// -1), so -3 can only come from here.
inline constexpr int kBundleFingerprintNoSlot = -3;
/// Tag-kind contribution when there is no tag to ask (null list element).
/// `Tag::Type` ids are small (< 12), so this can only come from here.
inline constexpr uint32_t kBundleFingerprintNullKind = 0xFFFFFFFFu;
/// Entry-hash contribution when there is no tag to hash (null list element).
inline constexpr uint64_t kBundleFingerprintNullHash = 0x9E3779B97F4A7C15ULL;

/// Mixes one `Items` entry into `seed`: stored slot, tag kind, entry hash.
[[nodiscard]] inline uint64_t fingerprintBundleEntry(uint64_t seed, int slot, uint32_t entryKind, uint64_t entryHash) {
    // FNV-1a 64: mix each field byte-wise. `seed` chains entries; start from
    // the FNV offset basis for the first entry.
    uint64_t hash = seed == 0 ? 14695981039346656037ULL : seed;
    auto     mix  = [&hash](uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(value >> (i * 8));
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<uint64_t>(static_cast<uint32_t>(slot)), 4);
    mix(static_cast<uint64_t>(entryKind), 4);
    mix(entryHash, 8);
    return hash;
}

/// Mixes one live Bundle entry (read from the client's dynamic container,
/// see BundlePreviewProvider) into `seed`: container index, item id, aux
/// value and count. Same FNV-1a chain as `fingerprintBundleEntry` so the two
/// paths share the tail mixer.
[[nodiscard]] inline uint64_t
fingerprintBundleLiveEntry(uint64_t seed, int index, short itemId, short aux, uint8_t count) {
    uint64_t hash = seed == 0 ? 14695981039346656037ULL : seed;
    auto     mix  = [&hash](uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(value >> (i * 8));
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<uint64_t>(static_cast<uint32_t>(index)), 4);
    mix(static_cast<uint64_t>(static_cast<uint16_t>(itemId)), 2);
    mix(static_cast<uint64_t>(static_cast<uint16_t>(aux)), 2);
    mix(static_cast<uint64_t>(count), 1);
    return hash;
}

/// Mixes the Bundle stack identity plus the entry count into `seed`.
[[nodiscard]] inline uint64_t
fingerprintBundleFinal(uint64_t seed, short bundleId, short bundleAux, uint8_t bundleCount, uint64_t entryCount) {
    uint64_t hash = seed == 0 ? 14695981039346656037ULL : seed;
    auto     mix  = [&hash](uint64_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(value >> (i * 8));
            hash *= 1099511628211ULL;
        }
    };
    mix(static_cast<uint64_t>(static_cast<uint16_t>(bundleId)), 2);
    mix(static_cast<uint64_t>(static_cast<uint16_t>(bundleAux)), 2);
    mix(static_cast<uint64_t>(bundleCount), 1);
    mix(entryCount, 8);
    return hash;
}

} // namespace lamium::inspection::preview

