#pragma once

#include "domain/bom_fields.h"
#include "presentation/table_format.h"

#include <array>

namespace bom {

enum class TreeColumn { Profile, LengthMm, UnitCost, Quantity, TotalCost, CostPerMeter, Count };

struct TreeColumnSpec
{
    TreeColumn column;
    ColumnInfo display;
    const NumericField* field{};
};

inline constexpr std::array treeColumns{
    TreeColumnSpec{TreeColumn::Profile, {"Building / Profile", Qt::AlignLeft | Qt::AlignVCenter}},
    TreeColumnSpec{TreeColumn::LengthMm, {"Length (mm)", Qt::AlignRight | Qt::AlignVCenter}, &fields::lengthMm},
    TreeColumnSpec{TreeColumn::UnitCost, {"Cost / piece", Qt::AlignRight | Qt::AlignVCenter}, &fields::cost},
    TreeColumnSpec{TreeColumn::Quantity, {"Quantity", Qt::AlignRight | Qt::AlignVCenter}, &fields::quantity},
    TreeColumnSpec{TreeColumn::TotalCost, {"Total cost", Qt::AlignRight | Qt::AlignVCenter}},
    TreeColumnSpec{TreeColumn::CostPerMeter, {"Cost / m", Qt::AlignRight | Qt::AlignVCenter}},
};

static_assert(treeColumns.size() == static_cast<std::size_t>(TreeColumn::Count));

constexpr const TreeColumnSpec* columnSpec(TreeColumn column)
{
    for (const auto& spec : treeColumns) {
        if (spec.column == column) {
            return &spec;
        }
    }
    return nullptr;
}

constexpr ColumnInfo columnInfo(TreeColumn column)
{
    const auto* spec = columnSpec(column);
    return spec ? spec->display : ColumnInfo{};
}

constexpr const NumericField* editableField(TreeColumn column)
{
    const auto* spec = columnSpec(column);
    return spec ? spec->field : nullptr;
}

}
