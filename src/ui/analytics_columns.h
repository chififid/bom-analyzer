#pragma once

#include "presentation/table_format.h"

namespace bom {

enum class TopItemColumn { Building, Profile, Position, TotalCost, Count };
enum class ProfileColumn { Name, TotalLength, TotalCost, AverageCost, Count };

constexpr ColumnInfo columnInfo(TopItemColumn column)
{
    switch (column) {
    case TopItemColumn::Building: return {"Building", Qt::AlignLeft | Qt::AlignVCenter};
    case TopItemColumn::Profile: return {"Profile", Qt::AlignLeft | Qt::AlignVCenter};
    case TopItemColumn::Position: return {"Position", Qt::AlignRight | Qt::AlignVCenter};
    case TopItemColumn::TotalCost: return {"Total cost", Qt::AlignRight | Qt::AlignVCenter};
    case TopItemColumn::Count: break;
    }
    return {};
}

constexpr ColumnInfo columnInfo(ProfileColumn column)
{
    switch (column) {
    case ProfileColumn::Name: return {"Profile", Qt::AlignLeft | Qt::AlignVCenter};
    case ProfileColumn::TotalLength: return {"Total length (m)", Qt::AlignRight | Qt::AlignVCenter};
    case ProfileColumn::TotalCost: return {"Total cost", Qt::AlignRight | Qt::AlignVCenter};
    case ProfileColumn::AverageCost: return {"Average cost / m", Qt::AlignRight | Qt::AlignVCenter};
    case ProfileColumn::Count: break;
    }
    return {};
}

}
