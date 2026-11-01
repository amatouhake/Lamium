#pragma once
#include <bitset>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace lamium::information {
// Bedrock slime chunks (BACKLOG L-232): a function of the chunk coordinates
// alone, the same in every world and on every server. Source and checks in
// docs/PROVENANCE.md; the game's own spawn rule is compared against it by the
// slime_trace build.
//
// The rule seeds MT19937 with x * 0x1f1f1f1f ^ z and takes its first output.
// That output needs only state words 0, 1 and 397, so the seeding stops at
// 397 instead of filling all 624 (the map asks for thousands of chunks).
inline std::uint32_t mt19937FirstOutput(std::uint32_t seed) {
    std::uint32_t word = seed, first = seed, second = 0;
    for (std::uint32_t i = 1; i <= 397; ++i) {
        word = 1812433253u * (word ^ (word >> 30)) + i;
        if (i == 1) second = word;
    }
    std::uint32_t y = (first & 0x80000000u) | (second & 0x7fffffffu);
    y = word ^ (y >> 1) ^ ((y & 1u) ? 0x9908b0dfu : 0u);
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    return y ^ (y >> 18);
}
inline bool isSlimeChunk(int chunkX, int chunkZ) {
    auto seed = static_cast<std::uint32_t>(chunkX) * 0x1f1f1f1fu ^ static_cast<std::uint32_t>(chunkZ);
    return mt19937FirstOutput(seed) % 10 == 0;
}
// Slime chunks only exist in the Overworld.
inline bool slimeChunksIn(int dimension) { return dimension == 0; }

// Answers per 32x32-chunk block, filled on first use. The world map asks for
// every chunk on screen each frame; a full screen far out is a few hundred
// blocks. Bounded: past the limit it starts over rather than growing.
class SlimeChunkCache {
    static constexpr int side = 32;
    static constexpr std::size_t limit = 1024;
    std::unordered_map<std::uint64_t, std::bitset<side * side>> blocks;
    static int floorDiv(int value) { return value >= 0 ? value / side : -((-value + side - 1) / side); }
public:
    bool operator()(int chunkX, int chunkZ) {
        int bx = floorDiv(chunkX), bz = floorDiv(chunkZ);
        auto key = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(bx)) << 32) | static_cast<std::uint32_t>(bz);
        auto found = blocks.find(key);
        if (found == blocks.end()) {
            if (blocks.size() >= limit) blocks.clear();
            std::bitset<side * side> bits;
            for (int z = 0; z < side; ++z)
                for (int x = 0; x < side; ++x)
                    bits[static_cast<std::size_t>(z * side + x)] = isSlimeChunk(bx * side + x, bz * side + z);
            found = blocks.emplace(key, bits).first;
        }
        return found->second[static_cast<std::size_t>((chunkZ - bz * side) * side + (chunkX - bx * side))];
    }
    std::size_t size() const { return blocks.size(); }
};
// Slime chunks in [x0, x1] by [z0, z1], with neighbors in a row joined, so
// the map draws one rectangle per run.
struct SlimeRun { int z, x0, x1; }; // chunks x0..x1 inclusive
template <class IsSlime>
std::vector<SlimeRun> slimeRuns(int x0, int x1, int z0, int z1, IsSlime&& isSlime) {
    std::vector<SlimeRun> runs;
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
            if (!isSlime(x, z)) continue;
            if (!runs.empty() && runs.back().z == z && runs.back().x1 == x - 1) runs.back().x1 = x;
            else runs.push_back({z, x, x});
        }
    return runs;
}
// The outline of the slime areas in [x0, x1] by [z0, z1]: only borders
// between a slime chunk and a chunk that is not, so touching chunks in any
// direction read as one area. In chunk-edge units: a horizontal edge lies
// on z = line from x = from to x = to (exclusive), a vertical one on x = line.
// before: the slime side is the one below line, so the map draws it inside.
struct SlimeEdge { bool horizontal; int line, from, to; bool before; };
template <class IsSlime>
std::vector<SlimeEdge> slimeEdges(int x0, int x1, int z0, int z1, IsSlime&& isSlime) {
    std::vector<SlimeEdge> edges;
    auto add = [&](bool horizontal, int line, int at, bool before) {
        if (!edges.empty()) {
            auto& last = edges.back();
            if (last.horizontal == horizontal && last.line == line && last.to == at && last.before == before) { ++last.to; return; }
        }
        edges.push_back({horizontal, line, at, at + 1, before});
    };
    for (int z = z0; z <= z1 + 1; ++z)
        for (int x = x0; x <= x1; ++x)
            if (bool before = isSlime(x, z - 1); before != isSlime(x, z)) add(true, z, x, before);
    for (int x = x0; x <= x1 + 1; ++x)
        for (int z = z0; z <= z1; ++z)
            if (bool before = isSlime(x - 1, z); before != isSlime(x, z)) add(false, x, z, before);
    return edges;
}
}
