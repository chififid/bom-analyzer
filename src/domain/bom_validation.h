#pragma once

#include "domain/bom_fields.h"
#include "domain/operation_result.h"

#include <cmath>

namespace bom {

[[nodiscard]] inline Result<std::monostate> validateBuildingId(const QString& buildingId)
{
    if (buildingId.trimmed().isEmpty()) {
        return Error{ErrorCode::InvalidData, 0, QString::fromLatin1(fields::buildingId),
            QStringLiteral("BuildingID must not be empty.")};
    }
    return std::monostate{};
}

[[nodiscard]] inline Result<std::monostate> validateItem(const ProfileItem& item)
{
    if (item.profileName.trimmed().isEmpty()) {
        return Error{ErrorCode::InvalidData, 0, QString::fromLatin1(fields::profileName),
            QStringLiteral("ProfileName must not be empty.")};
    }
    for (const auto* field : fields::numeric) {
        const double value = std::visit([&item](auto member) {
            return static_cast<double>(item.*member);
        }, field->member);
        const bool positive = field->rule == NumericRule::Positive;
        if (!std::isfinite(value) || (positive ? value <= 0 : value < 0)) {
            const auto name = QString::fromLatin1(field->name);
            const auto requirement = positive ? QStringLiteral("greater than zero") : QStringLiteral("non-negative");
            const auto message = field->isInteger() ? QStringLiteral("%1 must be %2.")
                : QStringLiteral("%1 must be finite and %2.");
            return Error{ErrorCode::InvalidData, 0, name, message.arg(name, requirement)};
        }
    }
    return std::monostate{};
}

}
