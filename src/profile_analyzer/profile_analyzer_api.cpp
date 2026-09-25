#include "profile_analyzer/profile_analyzer_api.h"
#include "profile_analyzer/profile_analysis.h"

#include <algorithm>

extern "C" int __cdecl AnalyzeProfile(const wchar_t* profileName,
    double* nominalHeight, double* weightPerMeter, wchar_t* profileType) noexcept
{
    using profile_analyzer::Status;

    if (nominalHeight) {
        *nominalHeight = 0;
    }
    if (weightPerMeter) {
        *weightPerMeter = 0;
    }
    if (profileType) {
        profileType[0] = L'\0';
    }
    if (!profileName || !nominalHeight || !weightPerMeter || !profileType) {
        return static_cast<int>(Status::InvalidArgument);
    }

    try {
        const auto result = profile_analyzer::analyzeProfile(profileName);
        if (const auto* error = std::get_if<profile_analyzer::Error>(&result)) {
            return static_cast<int>(error->code);
        }
        const auto& info = std::get<profile_analyzer::ProfileInfo>(result);
        if (info.profileType.size() >= profile_analyzer::profileTypeCapacity) {
            return static_cast<int>(Status::InternalError);
        }

        std::copy(info.profileType.begin(), info.profileType.end(), profileType);
        profileType[info.profileType.size()] = L'\0';
        *nominalHeight = info.nominalHeight;
        *weightPerMeter = info.weightPerMeter;
        return static_cast<int>(Status::Success);
    } catch (...) {
        // C++ exceptions must not escape into callers such as Delphi.
        return static_cast<int>(Status::InternalError);
    }
}
