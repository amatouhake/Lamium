#include "features/information/DebugLines.h"
void check(bool, char const*);
void debugLinesTests() {
    using namespace lamium::information;
    {
        // L-57: counts follow the fps line; missing ones are left out.
        DebugValues value;
        value.timing = FrameStatistics{60.0, 16.7};
        value.entities = 12;
        value.particles = 30;
        auto java = buildDebugColumns(value, DebugLabel::JavaF3, {});
        check(java.left.size() == 2 && java.left[1].text == "E: 12 | P: 30", "java style counts line skips a missing count");
        GameText game;
        game.counts = "Entities: 12";
        auto standard = buildDebugColumns(value, DebugLabel::GameStandard, game);
        check(standard.left.size() == 2 && standard.left[1].text == "Entities: 12", "game style shows the counts line");
        DebugValues none;
        check(buildDebugColumns(none, DebugLabel::JavaF3, {}).left.empty(), "no counts line without counts");
    }
    {
        // L-124: during FreeCamera the camera's lines are labeled and the player's follow them.
        DebugValues value;
        value.x = 10; value.y = 70; value.z = -5;
        value.yaw = 90.f; value.pitch = 0.f;
        value.body = DebugValues::Body{1, 64, 2, 0.f, 10.f};
        auto java = buildDebugColumns(value, DebugLabel::JavaF3, {});
        check(java.left.size() == 5 && java.left[0].text == "Camera XYZ: 10.0 / 70.0 / -5.0"
                  && java.left[1].text == "Player XYZ: 1.0 / 64.0 / 2.0" && java.left[2].text.starts_with("Block: 10 70 -5")
                  && java.left[3].text.starts_with("Camera Facing: west")
                  && java.left[4].text == "Player Facing: south (towards +Z) | Yaw/Pitch: 0.0 / 10.0",
              "java style shows camera then player position and facing");
        GameText game;
        game.coordinates = "Cam XYZ"; game.bodyCoordinates = "Player XYZ";
        game.facing = "Cam Facing"; game.bodyFacing = "Player Facing";
        auto standard = buildDebugColumns(value, DebugLabel::GameStandard, game);
        check(standard.left.size() == 4 && standard.left[1].text == "Player XYZ" && standard.left[3].text == "Player Facing",
              "game style puts each player line after the camera's");
        value.body.reset();
        check(buildDebugColumns(value, DebugLabel::JavaF3, {}).left[0].text == "XYZ: 10.0 / 70.0 / -5.0",
              "no camera label outside FreeCamera");
    }
    {
        DebugValues value;
        value.header = "Minecraft 1.26.51 · Lamium 0.1.3";
        value.timing = FrameStatistics{120.0, 8.3};
        value.ping = 24;
        value.renderDistance = 16;
        value.maxRenderDistance = 32;
        value.x = 101.3;
        value.y = 64.0;
        value.z = -31.7;
        value.yaw = 180.f;
        value.pitch = 12.3f;
        value.skyLight = 15;
        value.blockLight = 0;
        value.biome = "minecraft:plains";
        value.difficulty = 2;
        value.worldTime = 42 * 24000 + 1500;
        value.raining = false;
        value.dimension = "overworld";
        auto java = buildDebugColumns(value, DebugLabel::JavaF3, {});
        check(java.left.size() == 8 && java.left.front().text == "Minecraft 1.26.51 · Lamium 0.1.3",
              "java style starts with the header line");
        check(java.left[1].text == "120 fps (8.3 ms) | Ping 24 ms | Render distance 16",
              "java style joins fps, ping and render distance");
        check(java.left[2].text == "XYZ: 101.3 / 64.0 / -31.7", "java style formats the position");
        check(java.left[3].text == "Block: 101 64 -32 | Chunk: 6, -2", "java style formats block and chunk");
        check(java.left[4].text == "Facing: north (towards -Z) | Yaw/Pitch: 180.0 / 12.3",
              "yaw maps to the cardinal word and axis");
        check(java.left[5].text == "Client Light: 15 (sky 15, block 0)", "java style light line");
        check(java.left[6].text == "Biome: minecraft:plains | Difficulty: Normal", "java style biome line");
        check(java.left[7].text == "Day 42 · 07:30 | Clear | Last quarter", "java style day, clock, weather and moon");
        check(java.right.size() == 3 && java.right[0].text == "Client" && java.right[1].text == "Dimension: overworld"
              && java.right[2].text == "Render distance: 16 / 32",
              "java style right column shows the client head and settings");
        check(java.right.back().text.find("System") == std::string::npos,
              "no system block without system values");
    }
    {
        GameText game;
        game.perf = "PING";
        game.coordinates = "COORD";
        game.blockChunk = "BLOCKCHUNK";
        game.facing = "FACING";
        game.light = "LIGHT";
        game.biome = "BIOME";
        game.time = "TIME";
        game.lookAt = "LOOK";
        game.client = "CLIENT";
        game.dimension = "DIM";
        game.renderDistance = "DIST";
        game.visuals = "VIS";
        game.screen = "SCREEN";
        game.system = "SYSTEM";
        game.memory = "MEM";
        DebugValues value;
        value.header = "H";
        value.timing = FrameStatistics{60.0, 16.6};
        value.x = 1.0;
        value.y = 2.0;
        value.z = 3.0;
        DebugTarget target;
        target.identifier = "minecraft:grass_block";
        target.gameLines = {"d1", "d2", "d3"};
        target.javaLines = {"j1"};
        value.target = target;
        auto standard = buildDebugColumns(value, DebugLabel::GameStandard, game);
        check(standard.left.size() == 13, "game style uses the supplied lines plus the look block");
        check(standard.left[1].text == "60 fps (16.6 ms) | PING", "game style prepends fps to the perf text");
        check(standard.left[8].text == " " && standard.left[9].text == "LOOK"
              && standard.left[10].text == "minecraft:grass_block" && standard.left[11].text == "d1"
              && standard.left[12].text == "d2",
              "game style keeps the heading, the identifier and at most two detail lines");
        check(standard.right.size() == 8 && standard.right[0].text == "CLIENT" && standard.right[6].text == "SYSTEM"
              && standard.right[7].text == "MEM",
              "game style right column keeps client before system");
        GameText empty;
        auto bare = buildDebugColumns(value, DebugLabel::GameStandard, empty);
        check(bare.left.size() == 2 && bare.left[1].text == "60 fps (16.6 ms)",
              "missing game text omits lines and leaves no dangling separators");
        check(bare.right.empty(), "a missing client head hides the whole right column");
    }
    {
        DebugValues value;
        value.raining = true;
        value.worldTime = 0;
        auto java = buildDebugColumns(value, DebugLabel::JavaF3, {});
        check(java.left.size() == 1 && java.left[0].text == "Day 0 · 06:00 | Rain | Full moon",
              "java style weather and moon follow the time line");
        value.worldTime.reset();
        check(buildDebugColumns(value, DebugLabel::JavaF3, {}).left.empty(), "no lines without values");
    }
    // The right column stays at the screen edge and only moves down past long left lines.
    check(rightColumnOffset({100, 50, 50}, {80, 80}, 300, 12) == 0, "a wide screen keeps both columns at the top");
    check(rightColumnOffset({250, 50, 50}, {80, 80}, 300, 12) == 1, "a long first left line moves the right column one row down");
    check(rightColumnOffset({250, 250, 50}, {80}, 300, 12) == 2, "the column moves past every long left line");
    check(rightColumnOffset({250, 250}, {80, 80}, 300, 12) == 2, "below the left column the right column always fits");
    check(rightColumnOffset({}, {80}, 300, 12) == 0, "no left column, no offset");
}
