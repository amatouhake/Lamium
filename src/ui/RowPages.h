#pragma once
// The pages that replace the settings rows (Game language, Saved data, Block
// highlight, Enchantment order): one shape for what the screen does with them
// (DESIGN "Screens"). At most one is open; opening one closes the others; a
// click outside it closes it and goes on to the rows; losing focus calms it.
#include "ui/EnchantView.h"
#include "ui/HighlightView.h"
#include "ui/LanguageView.h"
#include "ui/SavedDataView.h"
#include <array>
namespace lamium::ui::row_pages {
struct Page {
    bool (*opened)();
    bool (*begin)();
    void (*reset)();
    void (*calm)();
    bool (*click)(float, float, bool);
    void (*key)(int);
    void (*wheel)(int);
    void (*release)();
    void (*render)(MinecraftUIRenderContext&, SettingsTable const&, glm::vec2);
    bool typing; // keeps the text keyboard (a search field)
};
inline constexpr std::array<Page, 4> pages{{
    {enchant_view::opened, enchant_view::begin, enchant_view::reset, enchant_view::calm, enchant_view::click, enchant_view::key,
     enchant_view::wheel, enchant_view::release, enchant_view::renderContent, false},
    {highlight_view::opened, highlight_view::begin, highlight_view::reset, highlight_view::calm, highlight_view::click, highlight_view::key,
     highlight_view::wheel, highlight_view::release, highlight_view::renderContent, true},
    {language_view::opened, language_view::begin, language_view::reset, language_view::calm, language_view::click, language_view::key,
     language_view::wheel, language_view::release, language_view::renderContent, false},
    {saved_data_view::opened, saved_data_view::begin, saved_data_view::reset, saved_data_view::calm, saved_data_view::click,
     saved_data_view::key, saved_data_view::wheel, saved_data_view::release, saved_data_view::renderContent, false},
}};
inline Page const* open() {
    for (auto const& page : pages) if (page.opened()) return &page;
    return nullptr;
}
inline bool any() { return open() != nullptr; }
inline void resetAll(Page const* keep = nullptr) { for (auto const& page : pages) if (&page != keep) page.reset(); }
inline void calmAll() { for (auto const& page : pages) page.calm(); }
// Opens `page` alone; true when it is open afterwards.
inline bool begin(Page const& page) { resetAll(&page); return page.begin(); }
inline Page const& enchant() { return pages[0]; }
inline Page const& highlight() { return pages[1]; }
inline Page const& language() { return pages[2]; }
inline Page const& savedData() { return pages[3]; }
}
