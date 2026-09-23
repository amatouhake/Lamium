#include "overlay/ShapeDocument.h"
#include <nlohmann/json.hpp>

namespace lamium::overlay {
namespace {
using Json = nlohmann::json;
int integer(Json const& value) {
    if (!value.is_number_integer()) throw std::invalid_argument("Expected an integer");
    auto number = value.get<double>();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
        throw std::out_of_range("Integer outside supported range");
    return static_cast<int>(number);
}
double number(Json const& value) {
    if (!value.is_number() || !std::isfinite(value.get<double>()))
        throw std::invalid_argument("Expected a finite number");
    return value.get<double>();
}
template<class Enum, size_t N> Enum named(Json const& value, std::array<std::string_view,N> const& names) {
    auto text = value.get<std::string>();
    auto found = std::find(names.begin(),names.end(),text);
    if (found == names.end()) throw std::invalid_argument("Unknown shape option");
    return static_cast<Enum>(found-names.begin());
}
constexpr std::array<std::string_view,3> types{"circle","cylinder","sphere"};
constexpr std::array<std::string_view,3> snaps{"block_center","block_corner","off"};
constexpr std::array<std::string_view,3> planes{"xz","xy","yz"};
constexpr std::array<std::string_view,2> styles{"face","line"};
constexpr std::array<std::string_view,4> colors{"cyan","yellow","pink","white"};
void triple(Json const& value) {
    if (!value.is_array() || value.size()!=3) throw std::invalid_argument("Expected three coordinates");
}
}
std::vector<ShapeDefinition> decodeShapes(std::string_view text) {
    if (text.size() > 1024*1024) throw std::length_error("Shape document exceeds size limit");
    auto root = Json::parse(text);
    if (!root.is_object() || integer(root.at("version")) != 1) throw std::invalid_argument("Unsupported shape document");
    auto const& entries = root.at("shapes");
    if (!entries.is_array() || entries.size()>32) throw std::length_error("Invalid shape collection size");
    std::vector<ShapeDefinition> result;
    for (auto const& entry : entries) {
        ShapeDefinition definition;
        definition.name = entry.at("name").get<std::string>();
        definition.dimension = integer(entry.at("dimension"));
        definition.visible = entry.at("visible").get<bool>();
        // Appearance fields arrived after version 1 files existed; absent means defaults.
        if (entry.contains("style")) definition.style = named<ShapeStyle>(entry.at("style"),styles);
        if (entry.contains("color")) definition.color = named<ShapeColor>(entry.at("color"),colors);
        auto const& geometry = entry.at("geometry");
        if (geometry.at("type") == "plane") {
            auto const& origin = geometry.at("origin"); triple(origin);
            definition.geometry = PlaneSpec{{integer(origin[0]),integer(origin[1]),integer(origin[2])},
                integer(geometry.at("width")),integer(geometry.at("depth")),integer(geometry.at("spacing")),
                named<Plane>(geometry.at("plane"),planes)};
        } else {
            auto const& center = geometry.at("center"); triple(center);
            definition.geometry = ShapeSpec{named<Shape>(geometry.at("type"),types),
                {number(center[0]),number(center[1]),number(center[2])},named<Snap>(geometry.at("snap"),snaps),
                number(geometry.at("radius")),integer(geometry.at("height"))};
        }
        result.push_back(std::move(definition));
    }
    ShapeCollection validation;
    validation.replace(result);
    return result;
}
std::string encodeShapes(std::vector<ShapeDefinition> const& definitions) {
    ShapeCollection validation;
    validation.replace(definitions);
    Json root{{"version",1},{"shapes",Json::array()}};
    for (auto const& definition : definitions) {
        Json geometry;
        if (auto spec = std::get_if<ShapeSpec>(&definition.geometry)) {
            geometry = {{"type",types.at(static_cast<size_t>(spec->shape))},
                {"center",{spec->center.x,spec->center.y,spec->center.z}},
                {"snap",snaps.at(static_cast<size_t>(spec->snap))},{"radius",spec->radius},{"height",spec->height}};
        } else {
            auto const& plane = std::get<PlaneSpec>(definition.geometry);
            geometry = {{"type","plane"},{"origin",{plane.origin.x,plane.origin.y,plane.origin.z}},
                {"width",plane.width},{"depth",plane.depth},{"spacing",plane.spacing},
                {"plane",planes.at(static_cast<size_t>(plane.plane))}};
        }
        root["shapes"].push_back({{"name",definition.name},{"dimension",definition.dimension},
            {"visible",definition.visible},{"style",styles.at(static_cast<size_t>(definition.style))},
            {"color",colors.at(static_cast<size_t>(definition.color))},{"geometry",std::move(geometry)}});
    }
    return root.dump(2) + "\n";
}
}
