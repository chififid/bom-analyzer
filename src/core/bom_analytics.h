#pragma once

#include "domain/analytics_result.h"
#include "domain/bom_document.h"
#include "domain/operation_result.h"

namespace bom {

[[nodiscard]] Result<ItemMetrics> calculateItemMetrics(const ProfileItem& item);

[[nodiscard]] Result<AnalyticsResult> calculateAnalytics(const BomDocument& document);

}
