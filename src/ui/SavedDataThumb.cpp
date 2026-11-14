#include "ui/SavedDataThumb.h"
#include "features/map/MapRegionFiles.h"
#include "features/map/MapThumbnail.h"
#include "ui/ScreenParts.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/renderer/TextureGroup.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/container/Blob.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/image/Image.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/core_graphics/ImageBuffer.h"
#include <vector>

namespace lamium::ui::data_thumb {
namespace {
namespace thumb = map::thumbnail;
ResourceLocation const& location() {
    // Never destroyed: its destructor is game code, which must not run while the process tears down.
    static auto const* texture = new ResourceLocation(Core::PathView("lamium/saveddata/map"), ResourceFileSystem::Raw);
    return *texture;
}
struct State {
    std::string key;
    std::filesystem::path layer; // The Overworld surface folder of the data's map.
    std::optional<thumb::Area> area;
    std::vector<std::uint32_t> pixels;
    int next = 0;         // The next of the area's nine regions to read.
    bool uploaded = false, dirty = false;
} state;
void unload() {
    if (!state.uploaded) return;
    try {
        if (auto* client = screen::client())
            if (auto group = client->getTextureGroup()) group->unloadTexture(location(), false);
    } catch (...) {}
    state.uploaded = false;
}
// The region written last: where the player was when this data was last used, as near as the files tell.
std::optional<map::RegionKey> latestRegion(std::filesystem::path const& layer) {
    std::error_code ignored;
    std::optional<map::RegionKey> latest;
    std::filesystem::file_time_type latestTime{};
    int files = 0;
    std::filesystem::directory_iterator it(layer, ignored), end;
    for (; !ignored && it != end && files < 4000; it.increment(ignored), ++files) {
        auto name = it->path().filename().u8string();
        auto region = map::regionOfFileName(std::string_view(reinterpret_cast<char const*>(name.data()), name.size()));
        if (!region) continue;
        auto time = it->last_write_time(ignored);
        if (ignored) { ignored.clear(); continue; }
        if (!latest || time > latestTime) { latest = region; latestTime = time; }
    }
    return latest;
}
void readNext() {
    if (!state.area || state.next >= thumb::regionsAcross * thumb::regionsAcross) return;
    int const column = state.next % thumb::regionsAcross, row = state.next / thumb::regionsAcross;
    ++state.next;
    try {
        static map::RegionCodec codec;
        auto key = state.area->region(column, row);
        auto read = map::readRegionFiles(state.layer / map::regionFileName(key), key, codec);
        if (!read.data) return;
        thumb::blit(state.pixels, map::shadeRegion(*read.data), column, row);
        state.dirty = true;
    } catch (...) {}
}
void upload(IClientInstance& client) {
    auto group = client.getTextureGroup();
    if (!group) return;
    mce::Image image(thumb::side, thumb::side, mce::ImageFormat::RGBA8Unorm, mce::ImageUsage::SRGB);
    image.mAlphaUsage = mce::AlphaUsage::Transparent;
    image.setRawImage(mce::Blob(reinterpret_cast<std::uint8_t const*>(state.pixels.data()), state.pixels.size() * sizeof(std::uint32_t)));
    cg::ImageBuffer buffer(std::move(image));
    if (state.uploaded) state.uploaded = group->updateTextureInPlace(location(), std::move(buffer));
    else { group->uploadTexture(location(), std::move(buffer)); state.uploaded = true; }
    state.dirty = false;
}
}
void show(std::filesystem::path const& mapFolder, std::string const& key) {
    if (key == state.key) return;
    unload();
    state = {};
    state.key = key;
    if (mapFolder.empty()) return;
    state.layer = mapFolder / map::layerFolder(map::surfaceLayer(0));
    if (auto center = latestRegion(state.layer)) {
        state.area = thumb::Area{*center};
        state.pixels = thumb::blank();
    }
}
bool draw(MinecraftUIRenderContext& context, ImageRect rect) {
    if (!state.area) return false;
    try {
        // A region a frame: opening a data with a large map never stalls the screen.
        readNext();
        if (state.dirty) upload(context.mClient);
        if (state.uploaded && !runtimeImage(context, location(), rect)) { state.uploaded = false; state.dirty = true; }
    } catch (...) {}
    return true;
}
std::optional<std::pair<float, float>> at(int x, int z) {
    return state.area ? state.area->at(x, z) : std::nullopt;
}
void reset() {
    unload();
    state = {};
}
}
