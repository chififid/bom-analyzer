#pragma once

#include "domain/bom_document.h"
#include "domain/operation_result.h"

#include <QByteArray>

namespace bom {

// UTF-8 CSV with fixed columns; see README.md for the accepted dialect.
[[nodiscard]] Result<BomDocument> parseBomCsv(const QByteArray& data);
[[nodiscard]] Result<BomDocument> readBomCsv(const QString& filePath);

}
