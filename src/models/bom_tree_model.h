#pragma once

#include "core/bom_analytics.h"
#include "models/bom_tree_columns.h"

#include <QAbstractItemModel>

namespace bom {

class BomTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    using Column = TreeColumn;

    explicit BomTreeModel(QObject* parent = nullptr);

    [[nodiscard]] Result<std::monostate> setDocument(BomDocument document);
    [[nodiscard]] const AnalyticsResult& analytics() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

signals:
    void analyticsChanged();
    void editRejected(const bom::Error& error);

private:
    enum class NodeType { Invalid, Building, Item };

    NodeType nodeType(const QModelIndex& index) const;
    std::size_t buildingRowFor(const QModelIndex& index) const;
    QModelIndex makeBuildingIndex(int row, int column) const;
    QModelIndex makeItemIndex(int row, int column, int buildingRow) const;

    QVariant cellValue(const QModelIndex& index) const;
    QVariant buildingValue(std::size_t buildingRow, Column column) const;
    QVariant itemValue(const ProfileItem& item, const ItemMetrics& metrics, Column column) const;
    QVariant formattedValue(const QModelIndex& index, int role) const;
    QVariant appearanceData(const QModelIndex& index, int role) const;

    BomDocument document_;
    AnalyticsResult analytics_;
};

}
