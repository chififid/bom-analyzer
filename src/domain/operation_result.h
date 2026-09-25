#pragma once

#include <QString>

#include <variant>

namespace bom {

enum class ErrorCode { InvalidData, NumericOverflow, InvalidCsv, InvalidEncoding, FileAccess, ExternalLibrary };

struct Error
{
    ErrorCode code;
    qsizetype line{}; // One-based record start; zero when no source line is available.
    QString field;   // Empty when the error concerns the whole operation or record.
    QString message;
};

template<typename T>
using Result = std::variant<T, Error>;

}
