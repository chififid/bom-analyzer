#pragma once

#include "domain/bom_document.h"

#include <array>
#include <variant>

namespace bom {

enum class NumericRule { Positive, NonNegative };

struct NumericField
{
    const char* name;
    std::variant<double ProfileItem::*, int ProfileItem::*> member;
    NumericRule rule;

    constexpr bool isInteger() const
    {
        return std::holds_alternative<int ProfileItem::*>(member);
    }
};

namespace fields {

inline constexpr char buildingId[] = "BuildingID";
inline constexpr char itemId[] = "ItemID";
inline constexpr char profileName[] = "ProfileName";

inline constexpr NumericField lengthMm{"Length_mm", &ProfileItem::lengthMm, NumericRule::Positive};
inline constexpr NumericField cost{"Cost", &ProfileItem::cost, NumericRule::NonNegative};
inline constexpr NumericField quantity{"Quantity", &ProfileItem::quantity, NumericRule::Positive};

inline constexpr std::array numeric{&lengthMm, &cost, &quantity};

}
}
