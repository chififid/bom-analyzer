#pragma once

#include "profile_analyzer/profile_analyzer_api.h"

#include <string>
#include <string_view>
#include <variant>

namespace profile_analyzer {

struct ProfileInfo
{
    std::wstring profileType;
    double nominalHeight{};
    double weightPerMeter{};
};

struct Error
{
    Status code;
    const char* message;
};

using AnalysisResult = std::variant<ProfileInfo, Error>;

AnalysisResult analyzeProfile(std::wstring_view name);

}
