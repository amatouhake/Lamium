#pragma once
#include "overlay/Geometry.h"
namespace lamium::overlay {
inline std::vector<Line> chunkBorders(Point position, int minY, int maxY) {
    if (!finite(position) || minY >= maxY || int64_t(maxY)-minY > 65536)
        throw std::invalid_argument("Invalid chunk border bounds");
    double x = std::floor(position.x/16)*16, z = std::floor(position.z/16)*16;
    checkedCoordinate(x); checkedCoordinate(x+16); checkedCoordinate(z); checkedCoordinate(z+16);
    auto box = wireBox({x,double(minY),z}, {x+16,double(maxY),z+16});
    std::vector<Line> result(box.begin(), box.end());
    int64_t y = static_cast<int64_t>(std::floor(double(minY)/16))*16+16;
    for (; y<maxY; y+=16) {
        double h = static_cast<double>(y);
        result.push_back({{x,h,z},{x+16,h,z}});
        result.push_back({{x+16,h,z},{x+16,h,z+16}});
        result.push_back({{x+16,h,z+16},{x,h,z+16}});
        result.push_back({{x,h,z+16},{x,h,z}});
    }
    return result;
}
}
