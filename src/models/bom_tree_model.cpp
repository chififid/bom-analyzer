#include "models/bom_tree_model.h"
#include "presentation/table_format.h"

#include <QFont>
#include <QMetaType>

#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

namespace bom {
namespace {

QVariant readField(const ProfileItem& item, const NumericField& field)
{
    return std::visit([&item](auto member) -> QVariant {
        return item.*member;
    }, field.member);
}

Result<std::monostate> writeField(ProfileItem& item, const NumericField& field, const QVariant& value)
{
    const auto name = QString::fromLatin1(field.name);
    bool converted = false;
    const double number = value.toDouble(&converted);
    if (!converted || !std::isfinite(number)) {
        return Error{ErrorCode::InvalidData, 0, name,
            QStringLiteral("Enter a finite number using a decimal point.")};
    }
    if (field.isInteger() && (number < std::numeric_limits<int>::min()
        || number > std::numeric_limits<int>::max() || std::trunc(number) != number)) {
        return Error{ErrorCode::InvalidData, 0, name,
            QStringLiteral("%1 must be an integer within the int range.").arg(name)};
    }

    std::visit([&item, number](auto member) {
        using Value = std::remove_cvref_t<decltype(item.*member)>;
        item.*member = static_cast<Value>(number);
    }, field.member);
    return std::monostate{};
}

}

BomTreeModel::BomTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

Result<std::monostate> BomTreeModel::setDocument(BomDocument document)
{
    if (!std::in_range<int>(document.boms.size())) {
        return Error{ErrorCode::InvalidData, 0, {}, QStringLiteral("Too many buildings to display.")};
    }
    for (const auto& building : document.boms) {
        if (!std::in_range<int>(building.items.size())) {
            return Error{ErrorCode::InvalidData, 0, {}, QStringLiteral("Too many positions in a building to display.")};
        }
    }

    auto calculated = calculateAnalytics(document);
    if (const auto* error = std::get_if<Error>(&calculated)) {
        return *error;
    }
    auto& report = std::get<AnalyticsResult>(calculated);
    if (!std::in_range<int>(report.profiles.size())) {
        return Error{ErrorCode::InvalidData, 0, {}, QStringLiteral("Too many profiles to display.")};
    }

    // Publish the document and its report together only after validation succeeds.
    beginResetModel();
    document_ = std::move(document);
    analytics_ = std::move(report);
    endResetModel();
    emit analyticsChanged();
    return std::monostate{};
}

const AnalyticsResult& BomTreeModel::analytics() const
{
    return analytics_;
}

QModelIndex BomTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }
    if (!parent.isValid()) {
        return makeBuildingIndex(row, column);
    }
    return makeItemIndex(row, column, parent.row());
}

QModelIndex BomTreeModel::parent(const QModelIndex& index) const
{
    if (nodeType(index) != NodeType::Item) {
        return {};
    }
    return makeBuildingIndex(static_cast<int>(buildingRowFor(index)), 0);
}

int BomTreeModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(document_.boms.size());
    }
    if (nodeType(parent) != NodeType::Building || parent.column() != 0
        || parent.row() >= static_cast<int>(document_.boms.size())) {
        return 0;
    }
    return static_cast<int>(document_.boms[static_cast<std::size_t>(parent.row())].items.size());
}

int BomTreeModel::columnCount(const QModelIndex&) const
{
    return columnIndex(Column::Count);
}

QVariant BomTreeModel::data(const QModelIndex& index, int role) const
{
    if (nodeType(index) == NodeType::Invalid) {
        return {};
    }

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return formattedValue(index, role);
    case Qt::EditRole:
        return cellValue(index);
    case Qt::TextAlignmentRole:
    case Qt::FontRole:
        return appearanceData(index, role);
    default:
        return {};
    }
}

bool BomTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !flags(index).testFlag(Qt::ItemIsEditable)) {
        return false;
    }

    const auto& field = *editableField(static_cast<Column>(index.column()));
    const auto buildingRow = buildingRowFor(index);
    const auto itemRow = static_cast<std::size_t>(index.row());
    const auto& current = document_.boms[buildingRow].items[itemRow];
    auto edited = current;
    const auto written = writeField(edited, field, value);
    if (const auto* error = std::get_if<Error>(&written)) {
        emit editRejected(*error);
        return false;
    }
    if (readField(edited, field) == readField(current, field)) {
        return true;
    }

    auto candidate = document_;
    candidate.boms[buildingRow].items[itemRow] = std::move(edited);

    auto calculated = calculateAnalytics(candidate);
    if (const auto* error = std::get_if<Error>(&calculated)) {
        emit editRejected(*error);
        return false;
    }
    document_ = std::move(candidate);
    analytics_ = std::move(std::get<AnalyticsResult>(calculated));

    // Editing does not change tree structure, so existing indexes remain valid.
    const QList<int> roles{Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole};
    emit dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(columnCount() - 1), roles);
    const auto total = makeBuildingIndex(static_cast<int>(buildingRow), columnIndex(Column::TotalCost));
    emit dataChanged(total, total, roles);
    emit analyticsChanged();
    return true;
}

// Internal ID 0 marks a building; an item stores its building's row + 1.
BomTreeModel::NodeType BomTreeModel::nodeType(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != this || index.column() >= columnCount()) {
        return NodeType::Invalid;
    }
    if (index.internalId() == 0) {
        return static_cast<std::size_t>(index.row()) < document_.boms.size()
            ? NodeType::Building : NodeType::Invalid;
    }
    const auto buildingRow = static_cast<std::size_t>(index.internalId() - 1);
    if (buildingRow >= document_.boms.size()
        || static_cast<std::size_t>(index.row()) >= document_.boms[buildingRow].items.size()) {
        return NodeType::Invalid;
    }
    return NodeType::Item;
}

std::size_t BomTreeModel::buildingRowFor(const QModelIndex& index) const
{
    return nodeType(index) == NodeType::Building
        ? static_cast<std::size_t>(index.row())
        : static_cast<std::size_t>(index.internalId() - 1);
}

QModelIndex BomTreeModel::makeBuildingIndex(int row, int column) const
{
    return createIndex(row, column, quintptr{0});
}

QModelIndex BomTreeModel::makeItemIndex(int row, int column, int buildingRow) const
{
    return createIndex(row, column, static_cast<quintptr>(buildingRow) + 1);
}

QVariant BomTreeModel::cellValue(const QModelIndex& index) const
{
    const auto column = static_cast<Column>(index.column());
    const auto buildingRow = buildingRowFor(index);
    if (buildingRow >= document_.boms.size()) {
        return {};
    }
    if (nodeType(index) == NodeType::Building) {
        return buildingValue(buildingRow, column);
    }

    const auto& building = document_.boms[buildingRow];
    const auto itemRow = static_cast<std::size_t>(index.row());
    if (itemRow >= building.items.size()) {
        return {};
    }
    const auto summary = analytics_.boms.constFind(building.buildingId);
    if (summary == analytics_.boms.cend()) {
        return {};
    }
    const auto& item = building.items[itemRow];
    const auto metrics = summary->itemMetrics.constFind(item.id);
    return metrics == summary->itemMetrics.cend() ? QVariant{} : itemValue(item, *metrics, column);
}

QVariant BomTreeModel::buildingValue(std::size_t buildingRow, Column column) const
{
    const auto& buildingId = document_.boms[buildingRow].buildingId;
    switch (column) {
    case Column::Profile: return buildingId;
    case Column::TotalCost: {
        const auto summary = analytics_.boms.constFind(buildingId);
        return summary == analytics_.boms.cend() ? QVariant{} : QVariant(summary->totalCost);
    }
    default: return {};
    }
}

QVariant BomTreeModel::itemValue(
    const ProfileItem& item, const ItemMetrics& metrics, Column column) const
{
    if (const auto* field = editableField(column)) {
        return readField(item, *field);
    }
    switch (column) {
    case Column::Profile: return item.profileName;
    case Column::TotalCost: return metrics.totalCost;
    case Column::CostPerMeter: return metrics.costPerMeter;
    default: return {};
    }
}

QVariant BomTreeModel::formattedValue(const QModelIndex& index, int role) const
{
    const QVariant value = cellValue(index);
    if (value.typeId() == QMetaType::Double) {
        return role == Qt::ToolTipRole
            ? formatPreciseDecimal(value.toDouble()) : formatDecimal(value.toDouble());
    }
    return value;
}

QVariant BomTreeModel::appearanceData(const QModelIndex& index, int role) const
{
    const auto column = static_cast<Column>(index.column());
    if (role == Qt::TextAlignmentRole) {
        return static_cast<int>(columnInfo(column).alignment);
    }
    if (role == Qt::FontRole && nodeType(index) == NodeType::Building) {
        QFont font;
        font.setBold(true);
        return font;
    }
    return {};
}

QVariant BomTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole
        || section < 0 || section >= columnCount()) {
        return {};
    }
    return QString::fromLatin1(columnInfo(static_cast<Column>(section)).title);
}

Qt::ItemFlags BomTreeModel::flags(const QModelIndex& index) const
{
    const auto type = nodeType(index);
    if (type == NodeType::Invalid) {
        return Qt::NoItemFlags;
    }
    auto flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (type == NodeType::Item && editableField(static_cast<Column>(index.column()))) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

}
