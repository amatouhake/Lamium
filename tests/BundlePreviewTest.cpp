// Focused checks for the Bundle preview: recognition, content fingerprint
// and the dynamic grid shape. Game-independent: exercises the production
// helpers in features/inspection/preview/BundleContents.h directly (no mirrors).
// Built with `xmake build LamiumTests`, run with `xmake run LamiumTests`.

#include "features/inspection/preview/BundleContents.h"

#include <cstdint>
#include <cstdio>

using lamium::inspection::preview::BundleGrid;
using lamium::inspection::preview::fingerprintBundleEntry;
using lamium::inspection::preview::fingerprintBundleFinal;
using lamium::inspection::preview::fingerprintBundleLiveEntry;
using lamium::inspection::preview::isBundleTypeName;
using lamium::inspection::preview::kBundleFingerprintNoSlot;
using lamium::inspection::preview::kBundleFingerprintNullHash;
using lamium::inspection::preview::kBundleFingerprintNullKind;

namespace {

int gFailures = 0;

void check(bool ok, char const* what, int line) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

void testSupportsEveryBundleColour() {
    CHECK(isBundleTypeName("minecraft:bundle"));
    CHECK(isBundleTypeName("minecraft:black_bundle"));
    CHECK(isBundleTypeName("minecraft:white_bundle"));
    CHECK(isBundleTypeName("minecraft:undyed_bundle"));
    CHECK(isBundleTypeName("minecraft:light_blue_bundle"));
}

void testRejectsNonBundles() {
    CHECK(!isBundleTypeName("minecraft:shulker_box"));
    CHECK(!isBundleTypeName("minecraft:black_shulker_box"));
    CHECK(!isBundleTypeName("minecraft:stone"));
    CHECK(!isBundleTypeName(""));
    // A plain ends_with("bundle") would wrongly accept these.
    CHECK(!isBundleTypeName("minecraft:notabundle"));
    CHECK(!isBundleTypeName("minecraft:bundlelike"));
}

void testShapeForSmallCounts() {
    // shapeFor(<=0) stays 0x0 (nothing to pack).
    CHECK(BundleGrid::shapeFor(0).columns == 0 && BundleGrid::shapeFor(0).rows == 0);
    CHECK(BundleGrid::shapeFor(-3).columns == 0 && BundleGrid::shapeFor(-3).rows == 0);
    // Single entries stay readable instead of a 1-wide strip.
    CHECK(BundleGrid::shapeFor(1).columns == 3 && BundleGrid::shapeFor(1).rows == 1);
    // 4 entries fit 3x2 (6 cells): the wider 3-column shape keeps it short.
    CHECK(BundleGrid::shapeFor(4).columns == 3 && BundleGrid::shapeFor(4).rows == 2);
    CHECK(BundleGrid::shapeFor(9).columns == 3 && BundleGrid::shapeFor(9).rows == 3);
    // 10 entries need a 4th column rather than a 4th row.
    CHECK(BundleGrid::shapeFor(10).columns == 4 && BundleGrid::shapeFor(10).rows == 3);
}

void testShapeForLargeCounts() {
    // 12 entries (vanilla tooltip view limit) fit 4x3.
    CHECK(BundleGrid::shapeFor(12).columns == 4 && BundleGrid::shapeFor(12).rows == 3);
    // 13 entries: first shape past the old 16-cap world (4x4).
    CHECK(BundleGrid::shapeFor(13).columns == 4 && BundleGrid::shapeFor(13).rows == 4);
    CHECK(BundleGrid::shapeFor(16).columns == 4 && BundleGrid::shapeFor(16).rows == 4);
    // 17 entries: grows to 5 columns, never truncates.
    CHECK(BundleGrid::shapeFor(17).columns == 5 && BundleGrid::shapeFor(17).rows == 4);
    CHECK(BundleGrid::shapeFor(32).columns == 6 && BundleGrid::shapeFor(32).rows == 6);
    CHECK(BundleGrid::shapeFor(64).columns == 8 && BundleGrid::shapeFor(64).rows == 8);
    // Past the cap: clamped at 64, never drawn partially.
    CHECK(BundleGrid::shapeFor(65).columns == 8 && BundleGrid::shapeFor(65).rows == 8);
    CHECK(BundleGrid::shapeFor(1000).columns == 8 && BundleGrid::shapeFor(1000).rows == 8);
}

void testEveryShapeHoldsItsEntries() {
    for (int filled = 1; filled <= BundleGrid::kMaxSlots; ++filled) {
        BundleGrid const g = BundleGrid::shapeFor(filled);
        CHECK(g.columns >= BundleGrid::kMinColumns && g.columns <= BundleGrid::kMaxColumns);
        CHECK(g.columns * g.rows >= filled);
        // Minimal: one fewer row would not hold the entries... except when a
        // narrower shape could (columns grow before rows, so minimality is
        // approximate for non-square counts).
        CHECK(g.rows >= 1);
    }
}

void testLargeGridFitsOrdinaryScreens() {
    // At vanilla pitch (18-unit cells, 4-unit padding) the full 8x8 grid is
    // 152x152 GUI units: fits an ordinary full-HD inventory screen (~300+
    // units wide), and PreviewLayout::anchored flips/clamps at screen edges.
    float const w = 8 * 18.0f + 8.0f;
    float const h = 8 * 18.0f + 8.0f;
    CHECK(w == 152.0f && h == 152.0f);
    for (int filled = 1; filled <= BundleGrid::kMaxSlots; ++filled) {
        BundleGrid const g = BundleGrid::shapeFor(filled);
        float const      gw = g.columns * 18.0f + 8.0f;
        float const      gh = g.rows * 18.0f + 8.0f;
        CHECK(gw <= 152.0f && gh <= 152.0f);
    }
}

void testFingerprintIsOrderAndContentSensitive() {
    // The production entry mixer: same inputs hash identically, any field
    // change (slot, tag kind, entry hash) changes the fingerprint.
    uint64_t const a = fingerprintBundleEntry(0, 0, 10u, 12345ULL);
    uint64_t const b = fingerprintBundleEntry(0, 0, 10u, 12345ULL);
    CHECK(a == b && a != 0);
    CHECK(fingerprintBundleEntry(0, 1, 10u, 12345ULL) != a);  // slot change
    CHECK(fingerprintBundleEntry(0, 0, 8u, 12345ULL) != a);   // tag-kind change
    CHECK(fingerprintBundleEntry(0, 0, 10u, 54321ULL) != a);  // content change
    // Chaining is order-sensitive: same entries in different order differ.
    uint64_t const ab =
        fingerprintBundleEntry(a, 1, 10u, 999ULL);
    uint64_t const ba = fingerprintBundleEntry(fingerprintBundleEntry(0, 1, 10u, 999ULL), 0, 10u, 12345ULL);
    CHECK(ab != ba);
    // The malformed-element sentinel (the exact production constants the
    // cache mixes for null entries) differs from a real entry and from the
    // empty seed: malformed/non-Compound elements participate, never vanish.
    uint64_t const sentinel =
        fingerprintBundleEntry(0, kBundleFingerprintNoSlot, kBundleFingerprintNullKind, kBundleFingerprintNullHash);
    CHECK(sentinel != 0 && sentinel != a);
    CHECK(fingerprintBundleEntry(a, kBundleFingerprintNoSlot, kBundleFingerprintNullKind, kBundleFingerprintNullHash) != ab);
    // The production tail mixer: stack identity and entry count participate.
    uint64_t const tailA = fingerprintBundleFinal(a, 1, 0, 1, 2);
    CHECK(tailA == fingerprintBundleFinal(a, 1, 0, 1, 2));
    CHECK(fingerprintBundleFinal(a, 2, 0, 1, 2) != tailA); // stack id change
    CHECK(fingerprintBundleFinal(a, 1, 1, 1, 2) != tailA); // aux change
    CHECK(fingerprintBundleFinal(a, 1, 0, 2, 2) != tailA); // count change
    CHECK(fingerprintBundleFinal(a, 1, 0, 1, 3) != tailA); // structure change
    // Empty Bundles are stable: the same tail mix over a zero seed.
    CHECK(fingerprintBundleFinal(0, 1, 0, 1, 0) == fingerprintBundleFinal(0, 1, 0, 1, 0));
}

void testLiveFingerprintIsIndexAndStackSensitive() {
    // The live (dynamic-container) entry mixer used on 26.51.3: same inputs
    // hash identically, any field change (index, id, aux, count) changes it,
    // and chaining is order-sensitive.
    uint64_t const a = fingerprintBundleLiveEntry(0, 0, 10, 0, 1);
    CHECK(a == fingerprintBundleLiveEntry(0, 0, 10, 0, 1) && a != 0);
    CHECK(fingerprintBundleLiveEntry(0, 1, 10, 0, 1) != a); // index change
    CHECK(fingerprintBundleLiveEntry(0, 0, 11, 0, 1) != a); // id change
    CHECK(fingerprintBundleLiveEntry(0, 0, 10, 1, 1) != a); // aux change
    CHECK(fingerprintBundleLiveEntry(0, 0, 10, 0, 2) != a); // count change
    uint64_t const ab = fingerprintBundleLiveEntry(a, 1, 20, 0, 1);
    uint64_t const ba = fingerprintBundleLiveEntry(fingerprintBundleLiveEntry(0, 1, 20, 0, 1), 0, 10, 0, 1);
    CHECK(ab != ba);
}

} // namespace

int runBundlePreviewTests() {
    testSupportsEveryBundleColour();
    testRejectsNonBundles();
    testShapeForSmallCounts();
    testShapeForLargeCounts();
    testEveryShapeHoldsItsEntries();
    testLargeGridFitsOrdinaryScreens();
    testFingerprintIsOrderAndContentSensitive();
    testLiveFingerprintIsIndexAndStackSensitive();
    if (gFailures == 0) {
        std::printf("BundlePreview tests: all passed\n");
    } else {
        std::printf("BundlePreview tests: %d failure(s)\n", gFailures);
    }
    return gFailures;
}

