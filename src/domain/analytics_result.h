#pragma once

#include "domain/bom_document.h"

#include <QHash>

#include <cstddef>
#include <vector>

namespace bom {

struct ItemMetrics
{
    double totalCost{};
    double totalLengthMm{};
    double costPerMeter{};
};

struct BomSummary
{
    double totalCost{};
    QHash<ItemId, ItemMetrics> itemMetrics;
};

struct ProfileSummary
{
    QString profileName;
    double totalLengthMm{};
    double totalCost{};
    double averageCostPerMeter{};
};

struct ExpensiveItem
{
    QString buildingId;
    QString profileName;
    ItemId itemId{ItemId::Invalid};
    std::size_t positionNumber{}; // One-based display number in the calculated snapshot, not identity.
    double totalCost{};
};

struct AnalyticsResult
{
    double overallCost{};
    QHash<QString, BomSummary> boms; // Keyed by BuildingID, independent of document order.
    std::vector<ProfileSummary> profiles;
    std::vector<ExpensiveItem> topExpensiveItems;
};

}
