#pragma once
// L-116 check: which recipes does the client hold, in a local world and on a
// server, and how do they describe their ingredients and results? Writes the
// whole table to a file. Only active with `xmake f --recipe_trace=y`.
namespace lamium::researchTrace::recipeTable {
void start();
void stop();
}
