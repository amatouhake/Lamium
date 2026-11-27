#include "input/GameKeymap.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/input/KeyboardRemappingLayout.h"
#include "mc/client/input/Keymapping.h"
#include "mc/client/options/IOptionRegistry.h"
#include <chrono>
#include <string>

namespace lamium::input {
std::vector<int> gameKeymapKeys(IClientInstance& client, char const* name) {
    auto layout = client.getOptions().getCurrentKeyboardRemapping();
    if (!layout) return {};
    auto const& mapped = layout->getKeymappingByAction(name).mKeys.get();
    if (mapped.size() > 4) return {};
    std::vector<int> keys;
    for (int code : mapped) if (code > 0) keys.push_back(code);
    return keys;
}
GameKeyCodes const& gameKeyCodes(IClientInstance& client) {
    static GameKeyCodes codes;
    static std::chrono::steady_clock::time_point read{};
    if (auto const now = std::chrono::steady_clock::now(); now - read > std::chrono::seconds(1)) {
        read = now;
        for (size_t i = 0; i < gameKeys.size(); ++i) codes[i] = gameKeymapKeys(client, std::string{gameKeys[i].name}.c_str());
    }
    return codes;
}
}
