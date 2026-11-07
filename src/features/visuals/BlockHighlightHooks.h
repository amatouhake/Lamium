#pragma once
// Block highlight glue (BACKLOG L-320): tints designated block types in the
// terrain mesh and rebuilds chunks when the list or switch changes. Pure
// rules are in BlockHighlight.h.
namespace lamium::visuals::highlight {
bool start();
void stop();
}
