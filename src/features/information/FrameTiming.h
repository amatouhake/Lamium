#pragma once
#include "features/information/FrameRateMeter.h"
namespace lamium::information {
void startFrameTiming();
void stopFrameTiming();
std::optional<FrameStatistics> frameStatistics();
}
