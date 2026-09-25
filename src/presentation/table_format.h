#pragma once

#include <QLocale>
#include <QString>
#include <Qt>

namespace bom {

struct ColumnInfo
{
    const char* title;
    Qt::Alignment alignment;
};

template<typename Column>
constexpr int columnIndex(Column column)
{
    return static_cast<int>(column);
}

inline QString formatDecimal(double value)
{
    return QString::number(value, 'f', 2);
}

inline QString formatPreciseDecimal(double value)
{
    return QString::number(value, 'g', QLocale::FloatingPointShortest);
}

}
