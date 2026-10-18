#include "features/visuals/ConnectedSprites.h"
#include "features/visuals/PackImage.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/block/BlockGraphics.h"
#include "mc/client/renderer/block/TextureItem.h"
#include "mc/client/renderer/texture/TextureAtlas.h"
#include "mc/client/renderer/texture/TextureAtlasResourceCallbacks.h"
#include "mc/client/renderer/texture/TextureUVCoordinateSet.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core/resource/ResourceLocationPair.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include "mc/deps/core_graphics/ImageDescription.h"
#include "mc/deps/core_graphics/TextureSetImageContainer.h"
#include "mc/deps/core_graphics/enums/TextureFormat.h"
#include "mc/deps/json/Value.h"
#include "mc/resources/ResourcePackManager.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <format>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lamium::visuals::connected::sprites {
namespace {
// How the atlas takes the sprites (L-164 spike, 2026-10-12): an added
// `texture_data` entry gets its own place; the atlas then asks the resource
// packs which one holds the image (giving up on -1) and loads it through the
// pair overload. The texture group's cache is not consulted.
struct Source {
    std::string path; // the base texture
    Rule rule;
    std::vector<Mask> masks;
};
struct Baked {
    std::vector<Source> sources;
    std::unordered_map<std::string, std::string> png;      // sprite path -> PNG file
    std::unordered_map<std::string, std::string> basePath; // sprite path -> base texture path
    // Vibrant Visuals: a sprite's texture set, naming the sprite as its color
    // and the base's MER/normal/height maps baked the same way (L-164,
    // 2026-10-12: without them sprite faces caught light flatly).
    std::unordered_map<std::string, std::string> textureSet; // sprite path -> .texture_set.json
};
std::mutex mutex;
Baked baked;                              // guarded by mutex
std::unordered_set<std::string> served;   // guarded by mutex; sprite paths loaded since the last atlas
struct Table {
    std::unordered_map<std::uint64_t, std::array<std::optional<Shift>, 256>> tiles;
};
std::atomic<std::shared_ptr<Table const>> table;
// Raised by each terrain atlas build and when it finishes; the client thread
// rebuilds the lookup, and retries a while if sprites were still missing.
std::atomic<bool> dirty{false};
// Client thread: retries left, ticks until the next one, sprites placed last time.
int retries = 0, wait = 0;
size_t lastPlaced = 0;

void log(std::string const& text) {
    try { Runtime::instance().self().getLogger().info("Connected Textures: {}", text); } catch (...) {}
}
std::string pathOf(ResourceLocation const& location) {
    try { return location.mPath->value; } catch (...) { return {}; }
}
std::uint64_t key(float u, float v) {
    auto q = [](float x) { return static_cast<std::uint32_t>(std::lround(x * 1048576.f)); };
    return (static_cast<std::uint64_t>(q(u)) << 32) | q(v);
}
void textures(Json::Value const& entry, std::set<std::string>& out) {
    auto add = [&](Json::Value const& value) {
        if (value.isString()) out.insert(value.asString(""));
        else if (value.isObject()) out.insert(value["path"].asString(""));
    };
    auto const& list = entry["textures"];
    if (list.isArray())
        for (Json::Value::ArrayIndex i = 0; i < list.size(); ++i) add(list[i]);
    else add(list);
}
std::string encode(std::vector<std::uint32_t> const& pixels, int width, int height) {
    std::string png;
    stbi_write_png_to_func([](void* context, void* data, int size) { static_cast<std::string*>(context)->append(static_cast<char*>(data), size); },
                           &png, width, height, 4, pixels.data(), width * 4);
    return png;
}
std::array<std::string, 4> const imageExtensions{".png", ".tga", ".jpg", ".jpeg"};
std::array<std::string, 1> const textureSetExtension{".texture_set.json"};
struct Pixels { std::vector<std::uint32_t> data; int width = 0, height = 0; };
// A map image named by a texture set, read through the packs like the atlas.
std::optional<Pixels> readImage(ResourcePackManager& packs, std::string const& path) {
    std::string bytes;
    if (!packs.load(ResourceLocation(Core::PathView(path)), bytes, imageExtensions) || bytes.empty()) return std::nullopt;
    int width = 0, height = 0, channels = 0;
    auto* decoded = stbi_load_from_memory(reinterpret_cast<stbi_uc const*>(bytes.data()), static_cast<int>(bytes.size()), &width,
                                          &height, &channels, 4);
    if (!decoded) return std::nullopt;
    Pixels out{std::vector<std::uint32_t>(static_cast<size_t>(width) * height), width, height};
    std::memcpy(out.data.data(), decoded, out.data.size() * 4);
    stbi_image_free(decoded);
    return out;
}
// Gives every sprite of `source` a texture set derived from the base's, with
// its maps baked like the color. Without a base texture set nothing is added.
int bakeTextureSets(ResourcePackManager& packs, Source const& source, Baked& next, bool sample) {
    std::string text;
    if (!packs.load(ResourceLocation(Core::PathView(source.path)), text, textureSetExtension) || text.empty()) return 0;
    if (sample) log(std::format("texture set of {}: {}", source.path, text.substr(0, 240)));
    auto json = nlohmann::json::parse(text, nullptr, false, true);
    if (json.is_discarded() || !json.contains("minecraft:texture_set") || !json["minecraft:texture_set"].is_object()) return 0;
    std::string directory(directoryOf(source.path));
    auto inDirectory = [&](std::string const& name) { return directory.empty() ? name : directory + "/" + name; };
    std::unordered_map<std::string, std::optional<Pixels>> maps;
    int layers = 0;
    for (auto mask : source.masks) {
        auto derived = json;
        auto& set = derived["minecraft:texture_set"];
        auto name = spriteName(source.path, mask);
        set["color"] = name;
        for (auto& [layer, value] : set.items()) {
            if (layer == "color" || !value.is_string()) continue;
            auto file = inDirectory(value.get<std::string>());
            auto [found, fresh] = maps.try_emplace(file);
            if (fresh) found->second = readImage(packs, file);
            auto const& image = found->second;
            if (!image || !bakeable(image->width, image->height)) continue; // keeps the base's map
            auto layerName = name + "_" + layer;
            auto layerPath = inDirectory(layerName);
            next.png[layerPath] = encode(bake(image->data.data(), image->width, image->height, source.rule, mask), image->width,
                                         image->height);
            next.basePath[layerPath] = file;
            value = layerName;
            ++layers;
        }
        next.textureSet[spritePath(source.path, mask)] = derived.dump();
    }
    return layers;
}
// Bakes the sprites of every covered texture and names them in `copy`.
void prepare(Json::Value const& root, Json::Value& copy, mce::TextureGroup& group, ResourcePackManager& packs) {
    std::set<std::string> paths;
    auto const& data = root["texture_data"];
    for (auto const& name : data.getMemberNames()) textures(data[name], paths);
    Baked next;
    int skipped = 0, layers = 0;
    for (auto const& path : paths) {
        auto rule = ruleForTexture(path);
        if (!rule) continue;
        auto image = readPackImage(packs, path);
        if (!image) { ++skipped; continue; }
        int width = image->width, height = image->height;
        if (!bakeable(width, height)) { ++skipped; continue; }
        auto const* pixels = reinterpret_cast<std::uint32_t const*>(image->rgba.data());
        Source source{path, *rule, variantMasks(*rule)};
        for (auto mask : source.masks) {
            auto sprite = spritePath(path, mask);
            next.png[sprite] = encode(bake(pixels, width, height, *rule, mask), width, height);
            next.basePath[sprite] = path;
            copy["texture_data"][spriteName(path, mask)]["textures"] = Json::Value(sprite);
        }
        // An entry for the base itself: the atlas places a file once, so its
        // place is the tile vanilla faces use (checked against "glass" below).
        copy["texture_data"][spriteName(path, 0)]["textures"] = Json::Value(path);
        try { layers += bakeTextureSets(packs, source, next, fileName(path) == "bookshelf"); } catch (...) {}
        next.sources.push_back(std::move(source));
    }
    log(std::format("terrain atlas: {} sprites for {} textures, {} texture sets with {} baked maps ({} skipped: missing, "
                    "not RGBA or larger than 64 px)",
                    next.png.size() - layers, next.sources.size(), next.textureSet.size(), layers, skipped));
    std::lock_guard lock(mutex);
    baked = std::move(next);
    served.clear();
}

LL_TYPE_INSTANCE_HOOK(SpriteAtlas, ll::memory::HookPriority::Normal, TextureAtlas, &TextureAtlas::_createAtlas,
    TextureAtlasResourceCallbacks, Json::Value const& root, std::function<void(TextureAtlasStatus const&)> status,
    std::function<void()> const finished, std::shared_ptr<mce::TextureGroup> group, cg::MipMapSupport const mip,
    ResourcePackManager& packs, std::shared_ptr<cg::ImageBuffer> outFinal,
    std::shared_ptr<std::unordered_map<ResourceLocation, cg::TextureSetImageContainer>> outSources) {
    if (!versionSensitiveAllowed(capabilities::Id::ConnectedTextures))
        return origin(root,std::move(status),finished,std::move(group),mip,packs,std::move(outFinal),std::move(outSources));
    bool terrain = false;
    try { terrain = group && root["texture_name"].asString("") == "atlas.terrain"; } catch (...) {}
    if (!terrain)
        return origin(root, std::move(status), finished, std::move(group), mip, packs, std::move(outFinal), std::move(outSources));
    Json::Value copy(root);
    try { prepare(root, copy, *group, packs); } catch (...) {
        log("preparing sprites failed; the atlas is built as vanilla");
        copy = root;
        std::lock_guard lock(mutex);
        baked = {};
    }
    dirty = true;
    std::function<void()> const done = [finished] {
        if (finished) finished();
        dirty = true;
    };
    return origin(copy, std::move(status), done, std::move(group), mip, packs, std::move(outFinal), std::move(outSources));
}
std::string const* pngExtension(gsl::span<std::string const> extensions) {
    for (auto const& e : extensions) if (e == ".png") return &e;
    return nullptr;
}
std::string const* setExtension(gsl::span<std::string const> extensions) {
    for (auto const& e : extensions) if (e == ".texture_set.json") return &e;
    return nullptr;
}
std::string withoutPng(std::string path) {
    for (std::string_view end : {std::string_view(".png"), std::string_view(".texture_set.json")})
        if (path.ends_with(end)) path.resize(path.size() - end.size());
    return path;
}
// Lookups for a sprite or a baked map answer with the base file's pack; a
// texture set is preferred where the caller accepts one, as for vanilla.
using PackIndex = std::pair<int, std::string_view>;
LL_TYPE_INSTANCE_HOOK(SpritePackIndex, ll::memory::HookPriority::Normal, ResourcePackManager,
    &ResourcePackManager::$getPackStackIndexOfResource, PackIndex, ResourceLocation const& location,
    gsl::span<std::string const> extensions) {
    auto result = origin(location, extensions);
    if (!versionSensitiveAllowed(capabilities::Id::ConnectedTextures)) return result;
    if (result.first >= 0) return result;
    auto const* png = pngExtension(extensions);
    auto const* set = setExtension(extensions);
    if (!png && !set) return result;
    std::string base;
    bool hasSet = false;
    {
        std::lock_guard lock(mutex);
        auto path = withoutPng(pathOf(location));
        auto found = baked.basePath.find(path);
        if (found == baked.basePath.end()) return result;
        base = found->second;
        hasSet = baked.textureSet.contains(path);
    }
    // The view must outlive the call: point it at the caller's own entry.
    std::string const* matched = set && hasSet ? set : png;
    if (!matched) return result;
    auto answer = origin(ResourceLocation(Core::PathView(base)), extensions);
    if (answer.first >= 0) result = {answer.first, std::string_view(*matched)};
    return result;
}
using LoadPair = bool (ResourcePackManager::*)(ResourceLocationPair const&, std::string&, gsl::span<std::string const>) const;
LL_TYPE_INSTANCE_HOOK(SpritePackLoad, ll::memory::HookPriority::Normal, ResourcePackManager,
    static_cast<LoadPair>(&ResourcePackManager::$load), bool, ResourceLocationPair const& pair, std::string& stream,
    gsl::span<std::string const> extensions) {
    if (!versionSensitiveAllowed(capabilities::Id::ConnectedTextures)) return origin(pair,stream,extensions);
    bool png = pngExtension(extensions) != nullptr, set = setExtension(extensions) != nullptr;
    if (png || set) {
        auto path = withoutPng(pathOf(*pair.mResourceLocation));
        std::lock_guard lock(mutex);
        if (set) {
            auto found = baked.textureSet.find(path);
            if (found != baked.textureSet.end()) {
                stream = found->second;
                return true;
            }
        }
        if (png) {
            auto found = baked.png.find(path);
            if (found != baked.png.end()) {
                stream = found->second;
                served.insert(path);
                return true;
            }
        }
    }
    return origin(pair, stream, extensions);
}
bool installed = false;
bool atlasInstalled = false, indexInstalled = false, loadInstalled = false;

Shift lowCorner(TextureUVCoordinateSet const& set) { return {std::min(set._u0, set._u1), std::min(set._v0, set._v1)}; }
}

std::optional<Shift> shift(float u, float v, Mask mask) {
    if (mask == 0 || mask > 255) return std::nullopt;
    auto current = table.load(std::memory_order_acquire);
    if (!current) return std::nullopt;
    auto found = current->tiles.find(key(u, v));
    if (found == current->tiles.end()) return std::nullopt;
    return found->second[mask];
}

bool refresh() {
    if (!installed) return false;
    bool again = dirty.exchange(false);
    if (!again) {
        if (retries <= 0 || --wait > 0) return false;
        --retries;
        wait = 30;
    }
    Baked snapshot;
    std::unordered_set<std::string> loaded;
    {
        std::lock_guard lock(mutex);
        snapshot.sources = baked.sources;
        loaded = served;
    }
    auto next = std::make_shared<Table>();
    size_t expected = 0, placed = 0;
    std::string check;
    try {
        for (auto const& source : snapshot.sources) {
            auto base = BlockGraphics::getTextureUVCoordinateSet(spriteName(source.path, 0), 0, 0);
            auto low = lowCorner(base);
            if (fileName(source.path) == "glass") {
                auto vanilla = lowCorner(BlockGraphics::getTextureUVCoordinateSet("glass", 0, 0));
                check = key(low.u, low.v) == key(vanilla.u, vanilla.v) ? "base places match" : "base places differ from vanilla";
            }
            auto& tile = next->tiles[key(low.u, low.v)];
            for (auto mask : source.masks) {
                ++expected;
                auto sprite = spritePath(source.path, mask);
                if (!loaded.contains(sprite)) continue;
                auto place = lowCorner(BlockGraphics::getTextureUVCoordinateSet(spriteName(source.path, mask), 0, 0));
                if (key(place.u, place.v) == key(low.u, low.v)) continue;
                tile[mask] = Shift{place.u - low.u, place.v - low.v};
                ++placed;
            }
        }
    } catch (...) {
        log("reading sprite places failed; faces stay vanilla");
        next = std::make_shared<Table>();
    }
    table.store(std::move(next), std::memory_order_release);
    // The atlas may still be loading its images: look again for a while
    // (every 30 ticks, 20 times).
    if (again) { retries = placed < expected ? 20 : 0; wait = 30; }
    if (placed == expected) retries = 0;
    bool changed = again || placed != lastPlaced;
    if (changed) log(std::format("{} of {} sprites in place{}", placed, expected, check.empty() ? "" : "; " + check));
    lastPlaced = placed;
    return changed;
}

bool start() {
    if (installed) return true;
    if (!atlasInstalled) atlasInstalled = SpriteAtlas::hook() == 0;
    if (atlasInstalled && !indexInstalled) indexInstalled = SpritePackIndex::hook() == 0;
    if (indexInstalled && !loadInstalled) loadInstalled = SpritePackLoad::hook() == 0;
    if (!atlasInstalled || !indexInstalled || !loadInstalled) {
        capabilities::hooks(capabilities::Id::ConnectedTextures,false);
        stop();
        return false;
    }
    installed = true;
    return true;
}
void stop() {
    if (atlasInstalled && SpriteAtlas::unhook()) atlasInstalled = false;
    if (indexInstalled && SpritePackIndex::unhook()) indexInstalled = false;
    if (loadInstalled && SpritePackLoad::unhook()) loadInstalled = false;
    table.store(nullptr);
    installed = atlasInstalled || indexInstalled || loadInstalled;
    if (installed) capabilities::fail(capabilities::Id::ConnectedTextures,capabilities::Reason::RemovalFailed);
}
}
