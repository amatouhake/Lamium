#pragma once
#include <cstdint>
#include <optional>
class IClientInstance;
namespace lamium::information {
// Negative transport values mean no measurement. Preserve genuine zero latency.
inline std::optional<std::int64_t> measuredPing(std::int64_t milliseconds) {
    if (milliseconds < 0) return {};
    return milliseconds;
}
std::optional<std::int64_t> connectionPing(IClientInstance&);
}
