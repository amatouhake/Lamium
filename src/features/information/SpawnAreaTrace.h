#pragma once
// Issue #32 check: does the client hold a chunk's hardcoded spawn areas, in a
// local world and on a server? Only active with `xmake f --spawn_area_trace=y`.
namespace lamium::information::spawnAreaTrace {
void start();
void stop();
}
