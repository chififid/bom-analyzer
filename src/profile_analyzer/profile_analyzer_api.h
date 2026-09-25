#pragma once

#include <cstddef>

namespace profile_analyzer {

inline constexpr std::size_t profileTypeCapacity = 32;

enum class Status : int {
    Success = 0,
    InvalidArgument = 1,
    InvalidFormat = 2,
    UnsupportedProfile = 3,
    InternalError = 4,
};

}

#ifdef _WIN32
// The caller owns distinct, valid buffers: a null-terminated input and
// at least profileTypeCapacity writable wchar_t elements for profileType.
// On failure, each non-null output is reset to zero or an empty string.
extern "C" int __cdecl AnalyzeProfile(const wchar_t* profileName,
    double* nominalHeight, double* weightPerMeter, wchar_t* profileType) noexcept;
#endif
