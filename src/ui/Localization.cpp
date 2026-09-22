#include "ui/Localization.h"
#include "ui/Translations.h"
#include "ll/api/memory/Hook.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/locale/OptionalString.h"

namespace lamium::ui {
namespace {
// Scope the native lookup override to our four action labels. All Minecraft
// and resource-pack strings continue through the original implementation.
LL_TYPE_INSTANCE_HOOK(ActionLabelHook, ll::memory::HookPriority::Normal, Localization,
    &Localization::_getSimple, OptionalString, std::string const& id) {
    if (id.starts_with("key.Lamium.")) {
        auto value = translations::find(id, *mCode);
        if (!value.empty()) {
            OptionalString result;
            result.valid = true;
            result.string = std::string(value);
            return result;
        }
    }
    return origin(id);
}
bool installed = false;
}
std::string translated(std::string_view key) {
    auto locale = getI18n().getCurrentLanguage();
    auto value = translations::find(key, *locale->mCode);
    return std::string(value.empty() ? key : value);
}
bool startLocalization() {
    if (installed) return true;
    installed = ActionLabelHook::hook(true) == 0;
    return installed;
}
void stopLocalization() {
    if (installed) { ActionLabelHook::unhook(true); installed = false; }
}
}
