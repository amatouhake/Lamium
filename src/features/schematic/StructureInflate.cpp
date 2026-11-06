#include "features/schematic/StructureInflate.h"
#include "ll/api/utils/CompressUtils.h"
#include <string_view>
#include <utility>

namespace lamium::schematic {
std::optional<std::string> inflateStructureBody(std::span<std::uint8_t const> bytes, std::size_t maxSize) {
    try {
        auto inflated = ll::utils::compress_utils::decompress(
            std::string_view{reinterpret_cast<char const*>(bytes.data()), bytes.size()},
            ll::utils::compress_utils::CompressFormat::Zlib, maxSize);
        if (!inflated) return std::nullopt;
        return std::move(*inflated);
    } catch (...) {
        return std::nullopt;
    }
}
}
