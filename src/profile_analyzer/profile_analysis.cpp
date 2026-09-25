#include "profile_analyzer/profile_analysis.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <optional>
#include <utility>

namespace profile_analyzer {
namespace {

constexpr std::wstring_view whitespace = L" \t\r\n\v\f";
constexpr double steelMassPerMeterPerSquareMm = 0.00785;

struct ProfileRule
{
    std::wstring_view type;
    double areaFactor;
    double defaultThicknessMm;
};

// Demonstration assumptions, not catalog section geometry.
constexpr std::array profileRules{
    ProfileRule{L"IPE", 2.0, 7.0},
    ProfileRule{L"HEA", 3.0, 8.0},
    ProfileRule{L"L", 2.0, 5.0},
};

std::wstring_view trim(std::wstring_view text)
{
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::wstring_view::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
}

std::optional<std::wstring> readProfileType(std::wstring_view& text)
{
    const auto typeLength = text.find_first_not_of(
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    if (typeLength == 0 || typeLength == std::wstring_view::npos
        || typeLength >= profileTypeCapacity) {
        return std::nullopt;
    }

    std::wstring type(text.substr(0, typeLength));
    for (auto& character : type) {
        if (character >= L'a' && character <= L'z') {
            character = static_cast<wchar_t>(character - L'a' + L'A');
        }
    }
    text = trim(text.substr(typeLength));
    return type;
}

std::optional<double> readPositiveNumber(std::wstring_view& text)
{
    const auto token = text.substr(0, text.find_first_not_of(L"0123456789."));
    if (token.empty()) {
        return std::nullopt;
    }
    std::string digits;
    digits.reserve(token.size());
    for (const auto character : token) {
        digits.push_back(static_cast<char>(character));
    }
    double value{};
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(),
        value, std::chars_format::fixed);
    if (error != std::errc{} || end != digits.data() + digits.size()
        || !std::isfinite(value) || value <= 0) {
        return std::nullopt;
    }
    text = trim(text.substr(token.size()));
    return value;
}

Error invalidFormat(const char* message)
{
    return {Status::InvalidFormat, message};
}

}

AnalysisResult analyzeProfile(std::wstring_view name)
{
    auto remaining = trim(name);
    auto type = readProfileType(remaining);
    if (!type) {
        return invalidFormat("Expected a profile type of 1 to 31 Latin letters followed by a size.");
    }

    const auto height = readPositiveNumber(remaining);
    if (!height) {
        return invalidFormat("The nominal height must be a finite positive decimal number.");
    }

    std::optional<double> additionalSize;
    if (!remaining.empty()) {
        const auto separator = remaining.front();
        if (separator != L'*' && separator != L'x' && separator != L'X' && separator != L'\u00d7') {
            return invalidFormat("Expected a dimension separator: *, x, X or multiplication sign.");
        }
        remaining = trim(remaining.substr(1));
        const auto value = readPositiveNumber(remaining);
        if (!value || !remaining.empty()) {
            return invalidFormat("Expected one finite positive additional size and no trailing text.");
        }
        additionalSize = *value;
    }

    const auto rule = std::find_if(profileRules.begin(), profileRules.end(),
        [&type](const ProfileRule& candidate) { return candidate.type == *type; });
    if (rule == profileRules.end()) {
        return Error{Status::UnsupportedProfile, "The profile type is not supported."};
    }
    const double thickness = additionalSize.value_or(rule->defaultThicknessMm);
    const double weight = (*height * steelMassPerMeterPerSquareMm) * thickness * rule->areaFactor;
    if (!std::isfinite(weight) || weight <= 0) {
        return invalidFormat("The estimated weight is outside the supported numeric range.");
    }
    return ProfileInfo{std::move(*type), *height, weight};
}

}
