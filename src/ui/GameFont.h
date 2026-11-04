#pragma once

#include "mc/client/gui/Font.h"
#include "mc/client/gui/FontHandle.h"
#include "mc/client/gui/FontRepository.h"

namespace lamium::ui {
// FontHandle::getFont is gone from the SDK (26.60). A real handle names its
// font by id in the repository; anything else draws with the handle's own font.
inline ::Font& fontOf(::FontHandle const& handle) {
    if (!handle.mIsDummyHandle) {
        if (auto repository = static_cast<Bedrock::NonOwnerPointer<::FontRepository> const&>(handle.mFontRepository)) {
            auto const& fonts = static_cast<std::vector<gsl::not_null<std::shared_ptr<::Font>>> const&>(repository->mLoadedFonts);
            std::uint64_t const id = handle.mFontId;
            if (id < fonts.size()) return *fonts[id].get();
        }
    }
    return *static_cast<std::shared_ptr<::Font> const&>(handle.mDefaultFont);
}
} // namespace lamium::ui
