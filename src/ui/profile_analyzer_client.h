#pragma once

#include "domain/operation_result.h"

namespace bom {

struct AnalyzedProfile
{
    QString type;
    double nominalHeight{};
    double weightPerMeter{};
};

Result<AnalyzedProfile> analyzeProfileFromDll(const QString& name, const QString& libraryPath = {});

}
