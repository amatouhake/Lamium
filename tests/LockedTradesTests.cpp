#include "features/inspection/LockedTradeIndex.h"
void check(bool, char const*);
void lockedTradesTests() {
    using namespace lamium::inspection::lockedTrades;
    std::vector<int> tiers{0, 0, 0, 1, 1, 2, 3, 3, 4};
    check(recipeAt(tiers, 0, 2) == 2u && recipeAt(tiers, 1, 0) == 3u && recipeAt(tiers, 3, 1) == 7u,
          "a tier's n-th row is that tier's n-th recipe in packet order");
    check(!recipeAt(tiers, 2, 1) && !recipeAt(tiers, 5, 0) && !recipeAt(tiers, 0, -1), "rows past a tier find nothing");
    std::vector<int> mixed{1, 0, 1};
    check(recipeAt(mixed, 1, 1) == 2u, "tiers out of order still count within their tier");
    auto right = tipBox(100, 100, 50, 20, 640, 360);
    check(right.x == 108 && right.y == 72, "the tip sits right of and above the pointer");
    auto flipped = tipBox(620, 10, 50, 20, 640, 360);
    check(flipped.x == 562 && flipped.y == 0, "near the right edge it flips left and stays on screen");
}
