#include "overlay/ShapeCollection.h"
void check(bool, char const*);
void shapeCollectionTests() {
    using namespace lamium::overlay;
    ShapeCollection collection(3, 40);
    ShapeDefinition block{"First", 0, true, ShapeSpec{Shape::Sphere, {-.1,5,2}, Snap::BlockCenter, 0, 1}};
    auto first = collection.add(block);
    auto const* cached = collection.find(first)->lines.data();
    collection.rename(first,"建築用の球");
    check(collection.find(first)->definition.name == "建築用の球" && collection.find(first)->lines.data() == cached,
        "UTF-8 shape rename preserves identity and cached geometry");
    for (auto bad : {std::string{},std::string("   "),std::string("line\nbreak"),std::string(129,'a')}) {
        bool failed = false;
        try { collection.rename(first,bad); } catch (std::invalid_argument const&) { failed=true; }
        check(failed && collection.find(first)->definition.name == "建築用の球",
            "invalid rename preserves the previous name");
    }
    collection.rename(first,"First");
    check(collection.find(first)->lines.size() == 12, "shape collection caches block surface geometry");
    block.name = "Second"; block.dimension = 1;
    auto second = collection.add(block);
    check(first != second, "shape identities are distinct");
    int count = 0;
    collection.forVisible(0, [&](auto id, auto const&) { check(id == first, "dimension filters visible shapes"); ++count; });
    check(count == 1, "only current dimension is drawn");
    collection.setVisible(first, false);
    count = 0;
    collection.forVisible(0, [&](auto, auto const&) { ++count; });
    check(count == 0 && collection.find(first)->lines.data() == cached, "hiding retains cached geometry");
    auto original = collection.find(first)->definition;
    auto oversized = original;
    std::get<ShapeSpec>(oversized.geometry).radius = 100;
    bool rejected = false;
    try { collection.edit(first, oversized); } catch (std::length_error const&) { rejected = true; }
    check(rejected && collection.find(first)->lines.data() == cached
        && collection.find(first)->definition.name == "First", "failed shape edit preserves previous definition and geometry");
    auto plane = ShapeDefinition{"Plane", 0, true, PlaneSpec{{0,0,0},2,1,1,Plane::XZ}};
    rejected = false;
    try { collection.add(plane); } catch (std::length_error const&) { rejected = true; }
    check(rejected && collection.entries().size() == 2, "aggregate line budget rejects without partial insertion");
    check(collection.remove(second) && !collection.remove(second), "removing a shape releases its identity once");
    auto third = collection.add(plane);
    check(third > second && collection.find(third)->lines.size() == 20, "removal frees geometry budget without ID reuse");
    collection.edit(first, ShapeDefinition{"Moved", 1, true, PlaneSpec{{10,20,30},1,1,1,Plane::XY}});
    check(collection.find(first)->definition.name == "Moved" && collection.find(first)->lines.front().from.x == 10,
        "editing retains identity and replaces definition and cache");
    collection.clear();
    check(collection.entries().empty(), "world exit clears all geometry");
    auto fresh = collection.add(block);
    check(fresh > third && !collection.find(first), "new session cannot resolve stale shape IDs");
}
