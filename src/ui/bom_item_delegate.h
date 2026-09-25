#pragma once

#include <QStyledItemDelegate>

namespace bom {

class BomItemDelegate : public QStyledItemDelegate
{
public:
    explicit BomItemDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override;
};

}
