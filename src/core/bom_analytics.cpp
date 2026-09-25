#include "core/bom_analytics.h"
#include "domain/bom_validation.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace bom {
namespace {

constexpr std::size_t topItemLimit = 3;

Error numericOverflow(const QString& message)
{
    return {ErrorCode::NumericOverflow, 0, {}, message};
}

std::optional<Error> updateProfileSummary(ProfileSummary& summary, const ItemMetrics& metrics)
{
    summary.totalCost += metrics.totalCost;
    if (!std::isfinite(summary.totalCost)) {
        return numericOverflow(QStringLiteral("Profile total cost is out of range."));
    }
    summary.totalLengthMm += metrics.totalLengthMm;
    if (!std::isfinite(summary.totalLengthMm)) {
        return numericOverflow(QStringLiteral("Profile total length is out of range."));
    }
    // Weight by total length; a simple mean of per-item rates would bias the result.
    summary.averageCostPerMeter = summary.totalCost / summary.totalLengthMm * 1000.0;
    if (!std::isfinite(summary.averageCostPerMeter)) {
        return numericOverflow(QStringLiteral("Average cost per metre is out of range."));
    }
    return std::nullopt;
}

void updateTopThree(std::vector<ExpensiveItem>& topItems, const ExpensiveItem& candidate)
{
    // Strict comparison preserves document order when costs tie.
    const auto position = std::find_if(topItems.begin(), topItems.end(),
        [&candidate](const ExpensiveItem& existing) {
            return candidate.totalCost > existing.totalCost;
        });

    if (position == topItems.end() && topItems.size() == topItemLimit) {
        return;
    }

    topItems.insert(position, candidate);
    if (topItems.size() > topItemLimit) {
        topItems.pop_back();
    }
}

}

Result<ItemMetrics> calculateItemMetrics(const ProfileItem& item)
{
    const auto validation = validateItem(item);
    if (const auto* error = std::get_if<Error>(&validation)) {
        return *error;
    }

    ItemMetrics metrics;
    metrics.totalCost = item.cost * item.quantity;
    if (!std::isfinite(metrics.totalCost)) {
        return numericOverflow(QStringLiteral("Item total cost is out of range."));
    }
    metrics.totalLengthMm = item.lengthMm * item.quantity;
    if (!std::isfinite(metrics.totalLengthMm)) {
        return numericOverflow(QStringLiteral("Item total length is out of range."));
    }
    metrics.costPerMeter = item.cost / item.lengthMm * 1000.0;
    if (!std::isfinite(metrics.costPerMeter)) {
        return numericOverflow(QStringLiteral("Item cost per metre is out of range."));
    }
    return metrics;
}

Result<AnalyticsResult> calculateAnalytics(const BomDocument& document)
{
    AnalyticsResult result;
    result.boms.reserve(static_cast<qsizetype>(document.boms.size()));
    result.topExpensiveItems.reserve(topItemLimit + 1);

    QSet<ItemId> itemIds;
    QHash<QString, std::size_t> profileIndices;

    for (const Bom& building : document.boms) {
        const auto validation = validateBuildingId(building.buildingId);
        if (const auto* error = std::get_if<Error>(&validation)) {
            return *error;
        }
        if (result.boms.contains(building.buildingId)) {
            return Error{ErrorCode::InvalidData, 0, QString::fromLatin1(fields::buildingId),
                QStringLiteral("BuildingID must be unique within the document.")};
        }
        BomSummary bomSummary;
        bomSummary.itemMetrics.reserve(static_cast<qsizetype>(building.items.size()));

        for (std::size_t itemIndex = 0; itemIndex < building.items.size(); ++itemIndex) {
            const ProfileItem& item = building.items[itemIndex];
            if (item.id == ItemId::Invalid || itemIds.contains(item.id)) {
                return Error{ErrorCode::InvalidData, 0, QString::fromLatin1(fields::itemId),
                    QStringLiteral("ItemID must be non-zero and unique within the document.")};
            }
            itemIds.insert(item.id);
            const auto calculated = calculateItemMetrics(item);
            if (const auto* error = std::get_if<Error>(&calculated)) {
                return *error;
            }
            const auto& metrics = std::get<ItemMetrics>(calculated);
            bomSummary.itemMetrics.insert(item.id, metrics);

            bomSummary.totalCost += metrics.totalCost;
            if (!std::isfinite(bomSummary.totalCost)) {
                return numericOverflow(QStringLiteral("BOM total cost is out of range."));
            }

            const auto existing = profileIndices.constFind(item.profileName);
            std::size_t profileIndex;
            if (existing == profileIndices.cend()) {
                profileIndex = result.profiles.size();
                profileIndices.insert(item.profileName, profileIndex);
                result.profiles.push_back(ProfileSummary{item.profileName});
            } else {
                profileIndex = existing.value();
            }

            if (const auto error = updateProfileSummary(result.profiles[profileIndex], metrics)) {
                return *error;
            }
            updateTopThree(result.topExpensiveItems,
                ExpensiveItem{building.buildingId, item.profileName, item.id, itemIndex + 1, metrics.totalCost});
        }

        result.overallCost += bomSummary.totalCost;
        if (!std::isfinite(result.overallCost)) {
            return numericOverflow(QStringLiteral("Overall cost is out of range."));
        }
        result.boms.insert(building.buildingId, std::move(bomSummary));
    }

    return result;
}

}
