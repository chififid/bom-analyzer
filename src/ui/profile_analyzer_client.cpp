#include "ui/profile_analyzer_client.h"
#include "profile_analyzer/profile_analyzer_api.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QScopeGuard>

#include <algorithm>
#include <array>
#include <cmath>

namespace bom {

Result<AnalyzedProfile> analyzeProfileFromDll(const QString& name, const QString& libraryPath)
{
#ifdef _WIN32
    using profile_analyzer::Status;
    // The C ABI would silently stop at an embedded null in a QString.
    if (name.contains(QChar(u'\0'))) {
        return Error{ErrorCode::InvalidData, 0, QStringLiteral("ProfileName"),
            QStringLiteral("The profile name must not contain a null character.")};
    }

    const auto path = libraryPath.isEmpty()
        ? QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ProfileAnalyzer.dll"))
        : QFileInfo(libraryPath).absoluteFilePath();
    QLibrary library(path);
    if (!library.load()) {
        return Error{ErrorCode::ExternalLibrary, 0, {},
            QStringLiteral("Could not load the profile analyzer: %1").arg(library.errorString())};
    }
    // QLibrary's destructor alone does not unload the module.
    const auto unload = qScopeGuard([&library] { library.unload(); });
    const auto analyze = reinterpret_cast<decltype(&AnalyzeProfile)>(library.resolve("AnalyzeProfile"));
    if (!analyze) {
        return Error{ErrorCode::ExternalLibrary, 0, {},
            QStringLiteral("The profile analyzer does not export AnalyzeProfile: %1").arg(library.errorString())};
    }

    const auto wideName = name.toStdWString();
    std::array<wchar_t, profile_analyzer::profileTypeCapacity> type{};
    double height{};
    double weight{};
    const int status = analyze(wideName.c_str(), &height, &weight, type.data());
    switch (static_cast<Status>(status)) {
    case Status::Success:
        break;
    case Status::InvalidFormat:
        return Error{ErrorCode::InvalidData, 0, QStringLiteral("ProfileName"),
            QStringLiteral("Invalid profile name or dimensions. Examples: IPE200, HEA160, L150*5.")};
    case Status::UnsupportedProfile:
        return Error{ErrorCode::InvalidData, 0, QStringLiteral("ProfileName"),
            QStringLiteral("This profile type is not supported by the analyzer.")};
    default:
        return Error{ErrorCode::ExternalLibrary, 0, {},
            QStringLiteral("The profile analyzer failed (status %1).").arg(status)};
    }

    const auto end = std::find(type.begin(), type.end(), L'\0');
    if (end == type.begin() || end == type.end()
        || !std::isfinite(height) || height <= 0 || !std::isfinite(weight) || weight <= 0) {
        return Error{ErrorCode::ExternalLibrary, 0, {},
            QStringLiteral("The profile analyzer returned an invalid result.")};
    }
    return AnalyzedProfile{QString::fromWCharArray(type.data(), static_cast<qsizetype>(end - type.begin())),
        height, weight};
#else
    Q_UNUSED(name);
    Q_UNUSED(libraryPath);
    return Error{ErrorCode::ExternalLibrary, 0, {}, QStringLiteral("Profile analysis requires Windows.")};
#endif
}

}
