#pragma once
// L-129 research: whether the server sends a villager's locked higher-tier
// trades. Dumps each UpdateTradePacket's offer data to logs/trade-<n>.snbt.
// Only active with `xmake f --trade_trace=y`.
namespace lamium::researchTrace::trade {
void start();
void stop();
}
