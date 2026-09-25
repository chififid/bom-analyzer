#pragma once

#include "domain/analytics_result.h"
#include "domain/operation_result.h"

namespace bom {

[[nodiscard]] Result<std::monostate> writeAnalyticsJson(
    const QString& filePath, const AnalyticsResult& report);

}
