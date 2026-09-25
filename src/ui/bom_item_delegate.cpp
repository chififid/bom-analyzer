#include "ui/bom_item_delegate.h"

#include "models/bom_tree_columns.h"

#include <QDoubleValidator>
#include <QLineEdit>
#include <QLocale>
#include <QSpinBox>

#include <limits>

namespace bom {

BomItemDelegate::BomItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* BomItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&,
    const QModelIndex& index) const
{
    if (!index.flags().testFlag(Qt::ItemIsEditable)) {
        return nullptr;
    }
    const auto column = static_cast<TreeColumn>(index.column());
    const auto* field = editableField(column);
    if (!field) {
        return nullptr;
    }
    if (field->isInteger()) {
        auto* editor = new QSpinBox(parent);
        editor->setRange(field->rule == NumericRule::Positive ? 1 : 0, std::numeric_limits<int>::max());
        editor->setAlignment(columnInfo(column).alignment);
        editor->setFrame(false);
        return editor;
    }
    auto* editor = new QLineEdit(parent);
    auto* validator = new QDoubleValidator(0, std::numeric_limits<double>::max(), -1, editor);
    auto locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    validator->setLocale(locale);
    validator->setNotation(QDoubleValidator::ScientificNotation);
    editor->setValidator(validator);
    editor->setAlignment(columnInfo(column).alignment);
    editor->setFrame(false);
    editor->setToolTip(QStringLiteral("Use a decimal point; exponent notation such as 1e-3 is supported."));
    return editor;
}

void BomItemDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    const auto value = index.data(Qt::EditRole);
    if (auto* spinBox = qobject_cast<QSpinBox*>(editor)) {
        spinBox->setValue(value.toInt());
        spinBox->selectAll();
    } else if (auto* lineEdit = qobject_cast<QLineEdit*>(editor)) {
        // Round-trip precision prevents an unchanged editor from rounding imported data.
        lineEdit->setText(QString::number(value.toDouble(), 'g', std::numeric_limits<double>::max_digits10));
        lineEdit->selectAll();
    }
}

void BomItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
    const QModelIndex& index) const
{
    if (auto* spinBox = qobject_cast<QSpinBox*>(editor)) {
        spinBox->interpretText();
        model->setData(index, spinBox->value(), Qt::EditRole);
    } else if (auto* lineEdit = qobject_cast<QLineEdit*>(editor)) {
        model->setData(index, lineEdit->text(), Qt::EditRole);
    }
}

}
