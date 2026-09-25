#include "models/bom_tree_model.h"
#include "ui/analytics_columns.h"
#include "ui/bom_item_delegate.h"
#include "ui/main_window.h"
#include "ui/profile_analyzer_client.h"
#include <QAbstractItemModelTester>
#include <QAction>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QtTest/QTest>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

using bom::columnIndex;
using Column = bom::TreeColumn;

class BomUiTest : public QObject
{
    Q_OBJECT
    std::unique_ptr<bom::MainWindow> window;
    QTreeView* tree{};
    bom::BomTreeModel* model{};
    QLabel* total{};
    QTableWidget* profiles{};
    QTableWidget* top{};

    QModelIndex cell(Column column, int row = 0, int building = 0) const
    {
        return model->index(row, columnIndex(column), model->index(building, 0));
    }

private slots:
    void init()
    {
        window = std::make_unique<bom::MainWindow>();
        const QString path = QFINDTESTDATA("data/bom_sample.csv");
        QVERIFY(!path.isEmpty());
        window->loadCsv(path);
        tree = window->findChild<QTreeView*>(QStringLiteral("bomTree"));
        total = window->findChild<QLabel*>(QStringLiteral("totalLabel"));
        profiles = window->findChild<QTableWidget*>(QStringLiteral("profilesTable"));
        top = window->findChild<QTableWidget*>(QStringLiteral("topItemsTable"));
        QVERIFY(tree && total && profiles && top);
        model = qobject_cast<bom::BomTreeModel*>(tree->model());
        QVERIFY(model);
        new QAbstractItemModelTester(model, QAbstractItemModelTester::FailureReportingMode::QtTest, model);
    }

    void cleanup()
    {
        window->close();
        window.reset();
    }

    void structureIdentityAndAtomicReplacement()
    {
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->parent(cell(Column::Profile)), model->index(0, 0));
        QCOMPARE(cell(Column::TotalCost, 0, 1).data(Qt::EditRole), QVariant(480.0));
        for (const auto column : {Column::LengthMm, Column::UnitCost, Column::Quantity}) {
            QVERIFY(model->flags(cell(column)).testFlag(Qt::ItemIsEditable));
        }
        QVERIFY(!model->flags(cell(Column::TotalCost)).testFlag(Qt::ItemIsEditable));
        bom::BomDocument source{{
            {QStringLiteral("B2"), {
                {QStringLiteral("A"), 1000, 10, 1, bom::ItemId{41}},
                {QStringLiteral("A"), 2000, 40, 1, bom::ItemId{7}},
            }},
            {QStringLiteral("B1"), {{QStringLiteral("B"), 1000, 50, 1, bom::ItemId{93}}}},
        }};
        QVERIFY(std::holds_alternative<std::monostate>(model->setDocument(source)));
        const QPersistentModelIndex previous(cell(Column::Profile));
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        auto invalid = source;
        invalid.boms[0].items[0].quantity = 0;
        QVERIFY(std::holds_alternative<bom::Error>(model->setDocument(invalid)));
        QVERIFY(previous.isValid());
        QCOMPARE(resets.count(), 0);
        QCOMPARE(model->analytics().overallCost, 100.0);
        std::swap(source.boms[0].items[0], source.boms[0].items[1]);
        std::swap(source.boms[0], source.boms[1]);
        QVERIFY(std::holds_alternative<std::monostate>(model->setDocument(source)));
        QVERIFY(!previous.isValid());
        QCOMPARE(resets.count(), 1);
        QCOMPARE(cell(Column::TotalCost, 0, 1).data(Qt::EditRole), QVariant(40.0));
        QCOMPARE(cell(Column::TotalCost, 1, 1).data(Qt::EditRole), QVariant(10.0));
        QVERIFY(model->setData(cell(Column::UnitCost, 0, 1), 60.0));
        QCOMPARE(cell(Column::TotalCost, 0, 1).data(Qt::EditRole), QVariant(60.0));
        QCOMPARE(cell(Column::TotalCost, 1, 1).data(Qt::EditRole), QVariant(10.0));
    }

    void editsNotifyWithoutReset_data()
    {
        QTest::addColumn<Column>("column");
        QTest::addColumn<double>("value");
        QTest::addColumn<double>("overallCost");
        QTest::addColumn<double>("costPerMeter");
        QTest::newRow("length") << Column::LengthMm << 3000.0 << 1560.0 << 100.0;
        QTest::newRow("price") << Column::UnitCost << 100.0 << 1160.0 << 100.0 / 6.0;
        QTest::newRow("quantity") << Column::Quantity << 4.0 << 2160.0 << 50.0;
        const double nextLength = std::nextafter(6000.0, 7000.0);
        QTest::newRow("small-length-change") << Column::LengthMm << nextLength
            << 1560.0 << 300.0 / nextLength * 1000;
    }

    void editsNotifyWithoutReset()
    {
        QFETCH(Column, column);
        QFETCH(double, value);
        QFETCH(double, overallCost);
        QFETCH(double, costPerMeter);
        QSignalSpy changes(model, &QAbstractItemModel::dataChanged);
        QSignalSpy reports(model, &bom::BomTreeModel::analyticsChanged);
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        const QPersistentModelIndex previous(cell(column));
        QVERIFY(model->setData(cell(column), value));
        QVERIFY(cell(column).data(Qt::EditRole).toDouble() == value);
        QCOMPARE(model->analytics().overallCost, overallCost);
        QCOMPARE(cell(Column::CostPerMeter).data(Qt::EditRole).toDouble(), costPerMeter);
        QCOMPARE(changes.count(), 2);
        QCOMPARE(reports.count(), 1);
        QCOMPARE(changes[0][0].value<QModelIndex>(), cell(Column::Profile));
        QCOMPARE(changes[0][1].value<QModelIndex>(), cell(Column::CostPerMeter));
        const auto buildingTotal = model->index(0, columnIndex(Column::TotalCost));
        QCOMPARE(changes[1][0].value<QModelIndex>(), buildingTotal);
        QCOMPARE(changes[1][1].value<QModelIndex>(), buildingTotal);
        for (const auto& change : changes) {
            const auto roles = change[2].value<QList<int>>();
            for (int role : {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole}) {
                QVERIFY(roles.isEmpty() || roles.contains(role));
            }
        }
        QVERIFY(model->setData(cell(column), value));
        QCOMPARE(reports.count(), 1);
        QCOMPARE(changes.count(), 2);
        QCOMPARE(resets.count(), 0);
        QCOMPARE(previous, QPersistentModelIndex(cell(column)));
        QVERIFY(!model->setData(model->index(0, 0), 10));
        QVERIFY(!model->setData(cell(column), value, Qt::DisplayRole));
    }

    void rejectedEditsPreserveDocument_data()
    {
        QTest::addColumn<Column>("column");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<bom::ErrorCode>("code");
        const auto invalid = bom::ErrorCode::InvalidData;
        QTest::newRow("zero-length") << Column::LengthMm << QVariant(0.0) << invalid;
        QTest::newRow("text") << Column::LengthMm << QVariant(QStringLiteral("abc")) << invalid;
        QTest::newRow("fractional-quantity") << Column::Quantity << QVariant(1.5) << invalid;
        QTest::newRow("quantity-range") << Column::Quantity
            << QVariant(static_cast<double>(std::numeric_limits<int>::max()) + 1) << invalid;
        QTest::newRow("overflow") << Column::UnitCost << QVariant(1e308) << bom::ErrorCode::NumericOverflow;
    }

    void rejectedEditsPreserveDocument()
    {
        QFETCH(Column, column);
        QFETCH(QVariant, value);
        QFETCH(bom::ErrorCode, code);
        // Inspect the model's error signal without opening the window's queued dialog.
        disconnect(model, &bom::BomTreeModel::editRejected, window.get(), nullptr);
        QSignalSpy rejected(model, &bom::BomTreeModel::editRejected);
        QSignalSpy changes(model, &QAbstractItemModel::dataChanged);
        QSignalSpy reports(model, &bom::BomTreeModel::analyticsChanged);
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        const QPersistentModelIndex previous(cell(column));
        const auto before = previous.data(Qt::EditRole);
        QVERIFY(!model->setData(cell(column), value));
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected[0][0].value<bom::Error>().code, code);
        QCOMPARE(cell(column).data(Qt::EditRole), before);
        QCOMPARE(model->analytics().overallCost, 1560.0);
        QCOMPARE(changes.count(), 0);
        QCOMPARE(reports.count(), 0);
        QCOMPARE(resets.count(), 0);
        QCOMPARE(previous, QPersistentModelIndex(cell(column)));
    }

    void decimalEditorPreservesPrecision_data()
    {
        QTest::addColumn<double>("length");
        QTest::newRow("fractional") << 1234.5678901234567;
        QTest::newRow("tiny") << 1e-200;
        QTest::newRow("large") << 1e200;
    }

    void decimalEditorPreservesPrecision()
    {
        QFETCH(double, length);
        bom::BomItemDelegate delegate;
        QVERIFY(model->setData(cell(Column::UnitCost), 0.0));
        QVERIFY(model->setData(cell(Column::LengthMm), length));
        const auto index = cell(Column::LengthMm);
        std::unique_ptr<QWidget> editor(delegate.createEditor(nullptr, {}, index));
        auto* line = qobject_cast<QLineEdit*>(editor.get());
        QVERIFY(line);
        delegate.setEditorData(line, index);
        QVERIFY(line->hasAcceptableInput());
        QVERIFY(line->text().toDouble() == length);
        QVERIFY(index.data(Qt::ToolTipRole).toString().toDouble() == length);
        QSignalSpy reports(model, &bom::BomTreeModel::analyticsChanged);
        delegate.setModelData(line, model, index);
        QVERIFY(index.data(Qt::EditRole).toDouble() == length);
        QCOMPARE(reports.count(), 0);
    }

    void quantityEditorCommitsOnEnter()
    {
        QCOMPARE(total->text(), QStringLiteral("Overall cost: 1560.00"));
        QCOMPARE(top->rowCount(), 3);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        tree->setCurrentIndex(cell(Column::Quantity));
        tree->setFocus();
        QTest::keyClick(tree, Qt::Key_F2);
        auto* spin = tree->findChild<QSpinBox*>();
        QVERIFY(spin);
        spin->selectAll();
        QTest::keyClicks(spin, "3");
        QCOMPARE(total->text(), QStringLiteral("Overall cost: 1560.00"));
        QTest::keyClick(spin, Qt::Key_Return);
        QTRY_COMPARE(total->text(), QStringLiteral("Overall cost: 1860.00"));
        const auto* topCost = top->item(0, columnIndex(bom::TopItemColumn::TotalCost));
        QVERIFY(topCost);
        QCOMPARE(topCost->text(), QStringLiteral("900.00"));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void decimalEditorCommitsOrCancels_data()
    {
        QTest::addColumn<Qt::Key>("key");
        QTest::addColumn<double>("length");
        QTest::addColumn<QString>("profileLength");
        QTest::newRow("escape") << Qt::Key_Escape << 6000.0 << QStringLiteral("12.00");
        QTest::newRow("enter") << Qt::Key_Return << 9000.0 << QStringLiteral("18.00");
    }

    void decimalEditorCommitsOrCancels()
    {
        QFETCH(Qt::Key, key);
        QFETCH(double, length);
        QFETCH(QString, profileLength);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
        tree->setCurrentIndex(cell(Column::LengthMm));
        tree->setFocus();
        QTest::keyClick(tree, Qt::Key_F2);
        auto* line = tree->findChild<QLineEdit*>();
        QVERIFY(line);
        line->selectAll();
        QTest::keyClicks(line, "9000");
        QCOMPARE(cell(Column::LengthMm).data(Qt::EditRole).toDouble(), 6000.0);
        QTest::keyClick(line, key);
        QTRY_COMPARE(cell(Column::LengthMm).data(Qt::EditRole).toDouble(), length);
        const auto* displayedLength = profiles->item(0, columnIndex(bom::ProfileColumn::TotalLength));
        QVERIFY(displayedLength);
        QCOMPARE(displayedLength->text(), profileLength);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

#ifdef Q_OS_WIN
    void analyzesSelectedProfile_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("accepted");
        QTest::newRow("success") << QStringLiteral("IPE200") << true;
        QTest::newRow("unsupported") << QStringLiteral("XYZ200") << false;
        QTest::newRow("embedded-null") << QStringLiteral("IPE200\0junk") << false;
    }

    void analyzesSelectedProfile()
    {
        QFETCH(QString, name);
        QFETCH(bool, accepted);
        bom::BomDocument document;
        document.boms.push_back({QStringLiteral("B1"), {{name, 1000, 10, 1, bom::ItemId{1}}}});
        QVERIFY(std::holds_alternative<std::monostate>(model->setDocument(document)));
        auto* action = window->findChild<QAction*>(QStringLiteral("analyzeProfileAction"));
        QVERIFY(action);
        QVERIFY(!action->isEnabled());
        tree->setCurrentIndex(model->index(0, 0));
        QVERIFY(!action->isEnabled());
        const auto selected = cell(Column::Quantity);
        tree->setCurrentIndex(selected);
        QVERIFY(action->isEnabled());
        QSignalSpy changes(model, &QAbstractItemModel::dataChanged);
        QSignalSpy reports(model, &bom::BomTreeModel::analyticsChanged);

        bool seen = false;
        QMessageBox::Icon icon = QMessageBox::NoIcon;
        QString text;
        QString details;
        QTimer::singleShot(0, window.get(), [&] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                seen = true;
                icon = dialog->icon();
                text = dialog->text();
                details = dialog->informativeText();
                dialog->accept();
            }
        });
        action->trigger();
        QVERIFY(seen);
        QCOMPARE(icon, accepted ? QMessageBox::Information : QMessageBox::Critical);
        QVERIFY(!text.isEmpty());
        if (accepted) {
            QCOMPARE(text, QStringLiteral("Profile: IPE200"));
            QVERIFY(details.contains(QStringLiteral("Type: IPE")));
            QVERIFY(details.contains(QStringLiteral("Nominal height: 200.00 mm")));
            QVERIFY(details.contains(QStringLiteral("Estimated weight: 21.98 kg/m")));
            QVERIFY(details.contains(QStringLiteral("Demonstration estimate")));
        }
        QCOMPARE(changes.count(), 0);
        QCOMPARE(reports.count(), 0);
        QCOMPARE(tree->currentIndex(), selected);
        QVERIFY(std::holds_alternative<std::monostate>(model->setDocument({})));
        QVERIFY(!action->isEnabled());
    }

    void missingProfileLibraryReturnsError()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto result = bom::analyzeProfileFromDll(QStringLiteral("IPE200"),
            directory.filePath(QStringLiteral("missing.dll")));
        const auto* error = std::get_if<bom::Error>(&result);
        QVERIFY(error);
        QCOMPARE(error->code, bom::ErrorCode::ExternalLibrary);
        QVERIFY(!error->message.isEmpty());
    }
#endif

    void exportsCurrentEdits()
    {
        QVERIFY(model->setData(cell(Column::Quantity), 3));
        QVERIFY(model->setData(cell(Column::LengthMm), 9000.0));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("report.json")));
        window->saveReport(file.fileName());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto json = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(json.value(QStringLiteral("overallCost")).toDouble(), 1860.0);
        const auto exportedProfiles = json.value(QStringLiteral("profiles")).toArray();
        QCOMPARE(exportedProfiles.size(), 3);
        QCOMPARE(exportedProfiles[0].toObject().value(QStringLiteral("totalLengthMm")).toDouble(), 27000.0);
    }

    void headerOnlyClearsTheWindow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("empty.csv")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray header = "BuildingID,ProfileName,Length_mm,Cost,Quantity\n";
        QCOMPARE(file.write(header), static_cast<qint64>(header.size()));
        file.close();
        window->loadCsv(file.fileName());
        QCOMPARE(model->rowCount(), 0);
        QCOMPARE(top->rowCount(), 0);
        QCOMPARE(profiles->rowCount(), 0);
        QCOMPARE(total->text(), QStringLiteral("Overall cost: 0.00"));
    }

    void failedImportsKeepTheWindow_data()
    {
        QTest::addColumn<QByteArray>("record");
        QTest::addColumn<QString>("expectedContext");
        QTest::newRow("missing-file") << QByteArray{} << QString{};
        QTest::newRow("invalid-csv") << QByteArray("B1,A,1000,10,1\nB1,B,1000,20,0\n")
            << QStringLiteral("Line: 3\nField: Quantity");
        QTest::newRow("overflow") << QByteArray("B1,A,1000,1e308,2\n") << QString{};
    }

    void failedImportsKeepTheWindow()
    {
        QFETCH(QByteArray, record);
        QFETCH(QString, expectedContext);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("invalid.csv")));
        if (!record.isEmpty()) {
            QVERIFY(file.open(QIODevice::WriteOnly));
            const auto contents = "BuildingID,ProfileName,Length_mm,Cost,Quantity\n" + record;
            QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
            file.close();
        }
        const auto title = window->windowTitle();
        const QPersistentModelIndex previous(cell(Column::Profile));
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        bool seen = false;
        QString context;
        QTimer::singleShot(0, window.get(), [&] {
            if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                seen = !dialog->text().isEmpty();
                context = dialog->informativeText();
                dialog->accept();
            }
        });
        window->loadCsv(file.fileName());
        QVERIFY(seen);
        QCOMPARE(context, expectedContext);
        QCOMPARE(resets.count(), 0);
        QVERIFY(previous.isValid());
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(window->windowTitle(), title);
        QCOMPARE(total->text(), QStringLiteral("Overall cost: 1560.00"));
    }
};

QTEST_MAIN(BomUiTest)
#include "bom_ui_test.moc"

