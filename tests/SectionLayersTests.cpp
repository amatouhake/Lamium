#include "features/schematic/SectionLayers.h"
void check(bool, char const*);
void sectionLayersTests() {
    using namespace lamium::schematic;
    using State = SectionLayers::State;
    constexpr int below = std::numeric_limits<int>::min(), above = std::numeric_limits<int>::max();
    Point first{0, 8, 0}, last{7, 15, 7};
    auto slab = [](LayerAxis axis, int low, int high) { return PlacedSlab{false, axis, low, high}; };
    check(sectionLayers(PlacedSlab{}, first, last).state == State::Whole, "a view of every layer shows the whole section");
    check(sectionLayers(slab(LayerAxis::Y, below, 20), first, last) == SectionLayers{},
          "a section wholly under the cut equals one shown by every layer");
    check(sectionLayers(slab(LayerAxis::Y, below, 7), first, last).state == State::Hidden, "a section past the cut is hidden");
    check(sectionLayers(slab(LayerAxis::Y, 16, above), first, last).state == State::Hidden, "and so is one before a high-side cut");
    auto part = sectionLayers(slab(LayerAxis::Y, below, 10), first, last);
    check(part == SectionLayers{State::Part, LayerAxis::Y, 8, 10}, "the cut inside a section gives its shown range");
    check(sectionLayers(slab(LayerAxis::Y, 10, 10), first, last) == SectionLayers{State::Part, LayerAxis::Y, 10, 10},
          "a single layer is a one-layer range");

    // Stepping an Up to cut from 20 to 21: only a section holding the boundary changes.
    Point upperFirst{0, 16, 0}, upperLast{7, 23, 7};
    check(sectionLayers(slab(LayerAxis::Y, below, 20), first, last) == sectionLayers(slab(LayerAxis::Y, below, 21), first, last),
          "a section under both cuts is unchanged by the step");
    check(sectionLayers(slab(LayerAxis::Y, below, 20), upperFirst, upperLast)
              != sectionLayers(slab(LayerAxis::Y, below, 21), upperFirst, upperLast),
          "the section holding the cut changes");
    check(sectionLayers(slab(LayerAxis::Y, below, 23), upperFirst, upperLast)
              == sectionLayers(slab(LayerAxis::X, below, 99), upperFirst, upperLast),
          "wholly shown sections are equal whatever the axis");
    check(sectionLayers(slab(LayerAxis::Y, below, 3), first, last) == sectionLayers(slab(LayerAxis::X, 40, above), first, last),
          "and so are hidden ones");
    check(sectionLayers(slab(LayerAxis::X, below, 3), first, last) != sectionLayers(slab(LayerAxis::Z, below, 3), first, last),
          "the same range on another axis is another set of cells");
    check(sectionLayers(slab(LayerAxis::Y, below, 20), Point{0, 9, 0}, Point{7, 8, 7}).state == State::Hidden,
          "a section with no cell in the placed box is hidden");
    // With a one-cell halo the section just above the cut also changes.
    check(sectionLayers(slab(LayerAxis::Y, below, 15), Point{0, 15, 0}, Point{7, 24, 7})
              != sectionLayers(slab(LayerAxis::Y, below, 14), Point{0, 15, 0}, Point{7, 24, 7}),
          "a widened range notices a neighbour layer turning hidden");

    // The renderer's value: section cells widened by the halo and clipped to
    // the placed box. A placement off the section grid at negative
    // coordinates: origin (-13, 60, -3), 20 by 10 by 6 placed.
    Size placed{20, 10, 6};
    Point origin{-13, 60, -3};
    auto around = [&](PlacedSlab const& s, Point sectionLow) { return sectionLayersAround(s, placed, origin, sectionLow, 8, 2); };
    PlacedSlab all{};
    check(around(all, {-16, 56, -8}) == SectionLayers{}, "every layer shown keeps every section whole");
    // Up to along X at placed offset 4 (world x -9): the section from x -16
    // holds it; the section from x -8 starts at offset 5 but its halo reaches
    // offset 3, so it changes too; the one from x 0 (offsets 13-) does not.
    auto upTo = [&](int cut) { return slab(LayerAxis::X, below, cut); };
    check(around(upTo(4), {-16, 56, -8}) != around(upTo(5), {-16, 56, -8}), "the section holding the cut changes with it");
    check(around(upTo(4), {-8, 56, -8}) != around(upTo(5), {-8, 56, -8}), "so does the next one within the halo");
    check(around(upTo(4), {0, 56, -8}) == around(upTo(5), {0, 56, -8}), "a section beyond the halo is unchanged");
    check(around(upTo(4), {0, 56, -8}).state == State::Hidden, "and stays hidden");
    check(around(all, {0, 56, -8}) != around(upTo(4), {0, 56, -8}), "turning All into Up to hides a far section");
    check(around(all, {-16, 56, -8}) != around(upTo(4), {-16, 56, -8}), "and cuts the near one");
    // Only stepping from offset 9 to 10 (world x -4 to -3): both layers' sections change.
    auto only = [&](int cut) { return slab(LayerAxis::X, cut, cut); };
    check(around(only(9), {-8, 56, -8}) != around(only(10), {-8, 56, -8}), "an Only step changes the section it leaves");
    check(around(only(9), {-16, 56, -8}) == around(only(10), {-16, 56, -8}), "a section far from both layers is unchanged");
    check(around(only(18), {0, 56, -8}) != around(only(19), {0, 56, -8}), "and the section it enters changes");
    check(around(only(12), {-16, 56, -8}).state == State::Hidden && around(only(1), {-16, 56, -8}).state == State::Part,
          "the first section's cells are offsets 0-2 plus its halo");
}
