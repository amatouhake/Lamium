#include "ui/Localization.h"
#include "ui/Translations.h"
#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"

namespace lamium::ui {
std::string translated(std::string_view key) {
    auto locale = getI18n().getCurrentLanguage();
    auto value = translations::find(key, *locale->mCode);
    return std::string(value.empty() ? key : value);
}
}
