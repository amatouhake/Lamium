#include "features/information/SlimeChunk.h"
#include <random>
#include <set>
#include <utility>
void check(bool, char const*);
void slimeChunkTests() {
    using namespace lamium::information;
    bool same = true;
    for (std::uint32_t seed : {0u, 1u, 5489u, 0x1f1f1f1fu, 0x80000000u, 0xffffffffu, 0xdeadbeefu}) {
        std::mt19937 reference(seed);
        same = same && mt19937FirstOutput(seed) == reference();
    }
    for (std::uint32_t seed = 0; seed < 4000; seed += 7) {
        std::mt19937 reference(seed * 2654435761u);
        same = same && mt19937FirstOutput(seed * 2654435761u) == reference();
    }
    check(same, "the shortened seeding gives MT19937's first output");
    // The chunks listed in issue #30 for x, z in [-5, 5] (checked 2026-10-19
    // against the issue; the L-232 item names the same eleven).
    std::set<std::pair<int, int>> listed{{-4, -1}, {-4, 2}, {-3, -2}, {-3, -1}, {-1, 0}, {0, -5},
                                         {0, -2}, {0, 4}, {1, -2}, {3, 0}, {3, 1}};
    std::set<std::pair<int, int>> found;
    for (int x = -5; x <= 5; ++x)
        for (int z = -5; z <= 5; ++z)
            if (isSlimeChunk(x, z)) found.insert({x, z});
    check(found == listed, "slime chunks near the origin match the documented list");
    int count = 0;
    for (int x = -200; x < 200; ++x)
        for (int z = -200; z < 200; ++z) count += isSlimeChunk(x, z);
    check(count > 14000 && count < 18000, "about one chunk in ten is a slime chunk");
    check(slimeChunksIn(0) && !slimeChunksIn(1) && !slimeChunksIn(2), "slime chunks are an Overworld thing");
    SlimeChunkCache cache;
    bool cached = true;
    for (int x = -70; x < 70; x += 3)
        for (int z = -70; z < 70; z += 5) cached = cached && cache(x, z) == isSlimeChunk(x, z);
    check(cached && cache(-33, 31) == isSlimeChunk(-33, 31), "the cache answers like the rule across block edges");
    check(cache.size() == 36, "the cache keeps one entry per 32-chunk block");
    auto runs = slimeRuns(0, 9, 0, 1, [](int x, int z) { return z == 0 ? (x >= 2 && x <= 4) || x == 9 : x == 0; });
    check(runs.size() == 3 && runs[0].z == 0 && runs[0].x0 == 2 && runs[0].x1 == 4 && runs[1].x0 == 9 && runs[1].x1 == 9
              && runs[2].z == 1 && runs[2].x0 == 0 && runs[2].x1 == 0,
          "neighboring slime chunks in a row draw as one run; rows never join");
    int cells = 0;
    for (auto const& run : slimeRuns(-5, 5, -5, 5, isSlimeChunk)) cells += run.x1 - run.x0 + 1;
    check(cells == 11, "runs cover exactly the slime chunks");
}
