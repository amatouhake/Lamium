#include "overlay/ShapeDocument.h"
#include <nlohmann/json.hpp>
void check(bool,char const*);
void shapeDocumentTests() {
    using namespace lamium::overlay;
    std::vector<ShapeDefinition> definitions;
    for (auto type : {Shape::Circle,Shape::Cylinder,Shape::Sphere})
        for (auto snap : {Snap::BlockCenter,Snap::BlockCorner,Snap::Off})
            definitions.push_back({"日本語の形状",1,false,ShapeSpec{type,{16777217.5,-3.25,7},snap,1,3}});
    for (auto plane : {Plane::XZ,Plane::XY,Plane::YZ})
        definitions.push_back({"Grid",0,true,PlaneSpec{{-10,20,30},3,4,2,plane}});
    definitions.push_back({"Cone",0,true,ShapeSpec{Shape::Cone,{1.5,2.5,3.5},Snap::BlockCenter,4,7,Axis::Y,0,0,false}});
    definitions.push_back({"Frustum",0,true,ShapeSpec{Shape::Frustum,{1.5,2.5,3.5},Snap::BlockCenter,4,6,Axis::Y,2,0,false}});
    definitions.push_back({"Ellipsoid",0,true,ShapeSpec{Shape::Ellipsoid,{1.5,2.5,3.5},Snap::BlockCenter,4,1,Axis::Y,0,2,false}});
    definitions.push_back({"Dome",0,true,ShapeSpec{Shape::Dome,{1.5,2.5,3.5},Snap::BlockCenter,4,1,Axis::Y,0,0,true}});
    definitions.push_back({"Sideways",0,true,ShapeSpec{Shape::Box,{1.5,2.5,3.5},Snap::BlockCenter,2,3,Axis::X,0,0,false}});
    definitions.front().style = ShapeStyle::Line;
    definitions.front().color = ShapeColor::Pink;
    auto encoded = encodeShapes(definitions);
    auto decoded = decodeShapes(encoded);
    check(decoded.size() == definitions.size() && encodeShapes(decoded) == encoded,
        "all shape kinds, snapping modes and planes round trip deterministically");
    check(std::get<ShapeSpec>(decoded.front().geometry).center.x == 16777217.5,
        "shape documents preserve double coordinate precision");
    check(decoded.front().name == "日本語の形状" && !decoded.front().visible && decoded.front().dimension == 1,
        "shape metadata round trips with UTF-8 names");
    check(decoded.front().style == ShapeStyle::Line && decoded.front().color == ShapeColor::Pink
        && decoded.back().style == ShapeStyle::Face && decoded.back().color == ShapeColor::Cyan,
        "shape appearance round trips");
    auto root = nlohmann::json::parse(encoded);
    {
        auto legacy = root;
        for (auto& entry : legacy["shapes"]) { entry.erase("style"); entry.erase("color"); }
        auto upgraded = decodeShapes(legacy.dump());
        check(upgraded.front().style == ShapeStyle::Face && upgraded.front().color == ShapeColor::Cyan,
            "files without appearance fields load with face style and default color");
    }
    check(!root["shapes"][0].contains("id") && !root["shapes"][0].contains("lines"),
        "runtime identity and derived geometry are not serialized");
    {
        auto legacy = root;
        for (auto& entry : legacy["shapes"]) {
            if (entry["geometry"]["type"] == "plane") continue;
            entry["geometry"].erase("axis");
            entry["geometry"].erase("topRadius");
            entry["geometry"].erase("heightRadius");
            entry["geometry"].erase("dome");
        }
        auto upgraded = decodeShapes(legacy.dump());
        bool defaults = true;
        for (auto const& shape : upgraded) {
            if (auto spec = std::get_if<ShapeSpec>(&shape.geometry))
                defaults = defaults && spec->axis == Axis::Y && spec->topRadius == 0 && spec->heightRadius == 0
                    && !spec->dome;
        }
        check(defaults, "files without preset fields load the legacy shape");
    }
    for (int scenario=0;scenario<10;++scenario) {
        auto bad = root;
        switch (scenario) {
        case 0: bad["version"]=2; break;
        case 1: bad["shapes"][0]["geometry"]["snap"]="unknown"; break;
        case 2: bad["shapes"][0]["dimension"]=1.5; break;
        case 3: bad["shapes"][0]["geometry"]["radius"]=100000; break;
        case 4: bad["shapes"][0]["visible"]="yes"; break;
        case 5: bad["shapes"][0]["geometry"]["center"]={1,2}; break;
        case 6: bad["shapes"][9]["geometry"]["origin"][0]=2147483648ull; break;
        case 7: bad["shapes"][9]["geometry"]["spacing"]=0; break;
        case 8: bad["shapes"][0]["style"]="blocks"; break;
        case 9: bad["shapes"][0]["color"]=3; break;
        }
        bool rejected=false;
        try { (void)decodeShapes(bad.dump()); } catch (std::exception const&) { rejected=true; }
        check(rejected,"invalid shape documents fail instead of coercing or truncating");
    }
    ShapeCollection current;
    auto id = current.add(definitions.front());
    auto lines = current.find(id)->lines.data();
    auto invalid = definitions;
    invalid.back().name.clear();
    bool rejected=false;
    try { current.replace(invalid); } catch (std::invalid_argument const&) { rejected=true; }
    check(rejected && current.entries().size()==1 && current.find(id)->lines.data()==lines,
        "failure in final imported entry preserves entire previous collection");
    current.replace(decoded);
    check(current.entries().size()==decoded.size() && !current.find(id),
        "successful replacement assigns fresh runtime IDs");
    check(decodeShapes(encodeShapes({})).empty(),"empty shape document round trips");
}
