// Text helpers: the plugin's texts follow the program language (PluginI18n.h, through
// PluginSDK.h). Every text a user reads is a key of lang/en.json; numbers are formatted here,
// in code, into named TrF arguments, never by a translation.
#pragma once

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "TextUtil.h"
#include "Settings.h"

#include <sal.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <string_view>

namespace GroundEffects {

namespace I18n = PluginSDK::I18n;

// printf for a TrF argument: Num("%.2f", ms). The format is code, never a translation.
inline std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[64];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// The default rules' stored English names and the keys of their shown names.
struct DefaultRuleName {
    const char* english;
    const char* key;
};
inline constexpr DefaultRuleName kDefaultRuleNames[] = {
    { kShockedGroundLabel, I18n::Key("rule_shocked_ground") },
    { kChilledGroundLabel, I18n::Key("rule_chilled_ground") },
    { kIgnitedGroundLabel, I18n::Key("rule_ignited_ground") },
    { kCausticCloudLabel, I18n::Key("rule_caustic_cloud") },
};

// A rule's name as shown: a default rule's stored English name in the program language while
// it is unchanged (display only), any other name as the user wrote it.
inline const char* ShownRuleLabel(const std::string& label) {
    for (const DefaultRuleName& d : kDefaultRuleNames)
        if (label == d.english) return I18n::Tr(d.key);
    return label.c_str();
}

// The name to store for a name typed in the settings tab: a default rule's shown name typed
// back is stored as its English name again, so it keeps following the program language.
inline std::string StoredRuleLabel(const std::string& shown) {
    for (const DefaultRuleName& d : kDefaultRuleNames)
        if (shown == I18n::Tr(d.key)) return d.english;
    return shown;
}

} // namespace GroundEffects
