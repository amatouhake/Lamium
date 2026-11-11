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
}
