#include "io/bom_json_writer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace bom {
namespace {

bool canExport(const AnalyticsResult& report)
{
    if (!std::isfinite(report.overallCost)) {
        return false;
    }
    for (const auto& building : report.boms) {
        if (!std::isfinite(building.totalCost)) {
            return false;
        }
    }
    for (const auto& profile : report.profiles) {
        if (!std::isfinite(profile.averageCostPerMeter) || !std::isfinite(profile.totalLengthMm)
            || !std::isfinite(profile.totalCost)) {
            return false;
        }
    }
    return true;
}

QJsonDocument reportJson(const AnalyticsResult& report)
{
    QJsonArray buildings;
    auto buildingIds = report.boms.keys();
    std::sort(buildingIds.begin(), buildingIds.end());
    for (const auto& buildingId : buildingIds) {
        const auto building = report.boms.constFind(buildingId);
        buildings.append(QJsonObject{
            {QStringLiteral("buildingId"), buildingId},
            {QStringLiteral("totalCost"), building->totalCost},
        });
    }
    QJsonArray profiles;
    for (const auto& profile : report.profiles) {
        profiles.append(QJsonObject{
            {QStringLiteral("profileName"), profile.profileName},
            {QStringLiteral("averageCostPerMeter"), profile.averageCostPerMeter},
            {QStringLiteral("totalLengthMm"), profile.totalLengthMm},
            {QStringLiteral("totalCost"), profile.totalCost},
        });
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("overallCost"), report.overallCost},
        {QStringLiteral("boms"), buildings},
        {QStringLiteral("profiles"), profiles},
    });
}

}

Result<std::monostate> writeAnalyticsJson(const QString& filePath, const AnalyticsResult& report)
{
    // JSON cannot represent non-finite numbers; Qt would silently write null.
    if (!canExport(report)) {
        return Error{ErrorCode::InvalidData, 0, {},
            QStringLiteral("The report contains a number that cannot be represented in JSON.")};
    }

    const auto contents = reportJson(report).toJson(QJsonDocument::Indented);
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return Error{ErrorCode::FileAccess, 0, {},
            QStringLiteral("Could not open the report for writing: %1").arg(filePath)};
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        return Error{ErrorCode::FileAccess, 0, {},
            QStringLiteral("Could not save the report: %1").arg(filePath)};
    }
    return std::monostate{};
}

}
