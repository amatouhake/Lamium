#include "features/inspection/TooltipGlyphs.h"
#include <string>
void check(bool, char const*);
void tooltipGlyphsTests() {
    using namespace lamium::inspection::tooltip;
    check(glyphLine(0).empty() && glyphLine(3) == "\n" + std::string(glyph) + std::string(glyph) + std::string(glyph),
          "the glyph line starts its own line");
    auto run = findGlyphRun("Bread" + glyphLine(3));
    check(run && run->line == 1 && run->count == 3, "the glyph line follows the name");
    auto longer = findGlyphRun("Golden Carrot\n§7Durability: 1 / 2\nlore" + glyphLine(7));
    check(longer && longer->line == 3 && longer->count == 7, "lines above the glyphs are counted");
    check(!findGlyphRun("Bread"), "no glyph line, nothing to paint");
    check(!findGlyphRun("Named " + std::string(glyph)), "a glyph inside the name is not Lamium's line");
    auto above = findGlyphRun("Bread\nlore" + glyphLine(2) + "\nminecraft:bread\nTags: a");
    check(above && above->line == 2 && above->count == 2, "Lamium's other lines may follow the glyph line");
    check(!findGlyphRun("Bread\nlore " + std::string(glyph) + "\nmore"), "a line with anything but glyphs is not ours");
    check(!findGlyphRun(std::string(glyph)), "a lone glyph without a line break is not ours");
    check(placeholders(0) == 0 && placeholders(1) == 3 && placeholders(3) == 7 && placeholders(7) == 15,
          "enough 4-unit blanks for icons 8 apart and 9 wide");
}
