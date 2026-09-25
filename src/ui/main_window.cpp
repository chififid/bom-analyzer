#include "ui/main_window.h"

#include "io/bom_csv_reader.h"
#include "io/bom_json_writer.h"
#include "models/bom_tree_model.h"
#include "presentation/table_format.h"
#include "ui/analytics_columns.h"
#include "ui/bom_item_delegate.h"
#include "ui/profile_analyzer_client.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <utility>

namespace bom {
namespace {

template<typename Column>
QTableWidget* makeTable(QWidget* parent)
{
    QStringList headers;
    for (int index = 0; index < columnIndex(Column::Count); ++index) {
        headers.push_back(QString::fromLatin1(columnInfo(static_cast<Column>(index)).title));
    }
    auto* table = new QTableWidget(0, columnIndex(Column::Count), parent);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setWordWrap(false);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    return table;
}

template<typename Column>
QTableWidgetItem* setCell(QTableWidget* table, int row, Column column, const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setToolTip(text);
    item->setTextAlignment(columnInfo(column).alignment);
    table->setItem(row, columnIndex(column), item);
    return item;
}

template<typename Column>
void setCell(QTableWidget* table, int row, Column column, double value)
{
    auto* item = setCell(table, row, column, formatDecimal(value));
    item->setToolTip(formatPreciseDecimal(value));
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , model_(new BomTreeModel(this))
    , tree_(new QTreeView(this))
    , fileLabel_(new QLabel(QStringLiteral("Open a CSV file to view its BOM and analytics."), this))
    , totalLabel_(new QLabel(this))
    , topItems_(makeTable<TopItemColumn>(this))
    , profiles_(makeTable<ProfileColumn>(this))
    , expandAction_(new QAction(QStringLiteral("Expand all"), this))
    , collapseAction_(new QAction(QStringLiteral("Collapse all"), this))
    , saveReportAction_(new QAction(QStringLiteral("Save report..."), this))
    , analyzeProfileAction_(new QAction(QStringLiteral("Analyze profile..."), this))
{
    setWindowTitle(QStringLiteral("BOM Analyzer"));
    resize(1120, 780);
    setMinimumSize(800, 560);

    setupTree();
    setupActionsAndMenus();
    setupLayout();
    refreshAnalytics();
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::setupActionsAndMenus()
{
    expandAction_->setObjectName(QStringLiteral("expandAllAction"));
    collapseAction_->setObjectName(QStringLiteral("collapseAllAction"));
    saveReportAction_->setObjectName(QStringLiteral("saveReportAction"));
    saveReportAction_->setShortcut(QKeySequence::Save);
    saveReportAction_->setEnabled(false);
    analyzeProfileAction_->setObjectName(QStringLiteral("analyzeProfileAction"));
    analyzeProfileAction_->setEnabled(false);

    auto* openAction = new QAction(QStringLiteral("Open CSV..."), this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::chooseCsv);
    connect(expandAction_, &QAction::triggered, tree_, &QTreeView::expandAll);
    connect(collapseAction_, &QAction::triggered, tree_, &QTreeView::collapseAll);
    connect(saveReportAction_, &QAction::triggered, this, &MainWindow::chooseReportPath);
    connect(analyzeProfileAction_, &QAction::triggered, this, &MainWindow::analyzeCurrentProfile);

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveReportAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close);
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(expandAction_);
    viewMenu->addAction(collapseAction_);
    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
    toolsMenu->addAction(analyzeProfileAction_);
    auto* toolbar = addToolBar(QStringLiteral("BOM actions"));
    toolbar->setMovable(false);
    toolbar->addAction(openAction);
    toolbar->addAction(saveReportAction_);
    toolbar->addSeparator();
    toolbar->addAction(expandAction_);
    toolbar->addAction(collapseAction_);
    toolbar->addSeparator();
    toolbar->addAction(analyzeProfileAction_);
}

void MainWindow::setupTree()
{
    tree_->setObjectName(QStringLiteral("bomTree"));
    tree_->setModel(model_);
    connect(tree_->selectionModel(), &QItemSelectionModel::currentChanged,
        this, &MainWindow::refreshProfileAction);
    connect(model_, &QAbstractItemModel::modelReset, this, &MainWindow::refreshProfileAction);
    tree_->setItemDelegate(new BomItemDelegate(tree_));
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QTreeView::customContextMenuRequested, this, &MainWindow::showTreeContextMenu);
    connect(model_, &BomTreeModel::analyticsChanged, this, [this] {
        refreshAnalytics();
        statusBar()->showMessage(QStringLiteral("Analytics updated. Changes are kept in memory; the source CSV is unchanged."));
    });
    // Wait until the delegate has finished closing its editor before opening a dialog.
    connect(model_, &BomTreeModel::editRejected, this, [this](const Error& error) {
        showError(error, QStringLiteral("Could not edit position"));
    }, Qt::QueuedConnection);
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(columnIndex(TreeColumn::Profile), QHeaderView::Stretch);
    tree_->header()->setStretchLastSection(false);
}

void MainWindow::showTreeContextMenu(const QPoint& position)
{
    const auto index = tree_->indexAt(position);
    if (index.isValid()) {
        tree_->setCurrentIndex(index);
    }

    QMenu menu(this);
    if (index.parent().isValid()) {
        menu.addAction(analyzeProfileAction_);
        menu.addSeparator();
        for (const auto& spec : treeColumns) {
            if (!spec.field) {
                continue;
            }
            auto* action = menu.addAction(QStringLiteral("Edit %1...")
                .arg(QString::fromLatin1(spec.display.title)));
            action->setData(columnIndex(spec.column));
        }
        menu.addSeparator();
    } else if (index.isValid()) {
        const auto building = index.siblingAtColumn(columnIndex(TreeColumn::Profile));
        menu.addAction(QStringLiteral("Expand building"), tree_, [this, building] { tree_->expand(building); });
        menu.addAction(QStringLiteral("Collapse building"), tree_, [this, building] { tree_->collapse(building); });
        menu.addSeparator();
    }
    menu.addAction(expandAction_);
    menu.addAction(collapseAction_);
    if (const auto* action = menu.exec(tree_->viewport()->mapToGlobal(position));
        action && action->data().isValid()) {
        const auto cell = index.siblingAtColumn(action->data().toInt());
        tree_->setCurrentIndex(cell);
        tree_->edit(cell);
    }
}

void MainWindow::refreshProfileAction()
{
    analyzeProfileAction_->setEnabled(tree_->currentIndex().parent().isValid());
}

void MainWindow::analyzeCurrentProfile()
{
    const auto index = tree_->currentIndex();
    if (!index.parent().isValid()) {
        return;
    }
    const auto name = index.siblingAtColumn(columnIndex(TreeColumn::Profile)).data(Qt::EditRole).toString();
    const auto analyzed = analyzeProfileFromDll(name);
    if (const auto* error = std::get_if<Error>(&analyzed)) {
        showError(*error, QStringLiteral("Could not analyze profile"), name);
        return;
    }

    const auto& profile = std::get<AnalyzedProfile>(analyzed);
    QMessageBox dialog(QMessageBox::Information, QStringLiteral("Profile analysis"),
        QStringLiteral("Profile: %1").arg(name), QMessageBox::Ok, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setInformativeText(QStringLiteral("Type: %1\nNominal height: %2 mm\nEstimated weight: %3 kg/m\n\n"
        "Demonstration estimate; not a catalog mass.")
        .arg(profile.type, formatDecimal(profile.nominalHeight), formatDecimal(profile.weightPerMeter)));
    dialog.setToolTip(QStringLiteral("Nominal height: %1 mm\nEstimated weight: %2 kg/m")
        .arg(formatPreciseDecimal(profile.nominalHeight), formatPreciseDecimal(profile.weightPerMeter)));
    dialog.exec();
}

void MainWindow::setupLayout()
{
    fileLabel_->setObjectName(QStringLiteral("fileLabel"));
    totalLabel_->setObjectName(QStringLiteral("totalLabel"));
    topItems_->setObjectName(QStringLiteral("topItemsTable"));
    profiles_->setObjectName(QStringLiteral("profilesTable"));
    fileLabel_->setTextFormat(Qt::PlainText);
    fileLabel_->setWordWrap(true);
    auto totalFont = totalLabel_->font();
    totalFont.setPointSize(totalFont.pointSize() + 3);
    totalFont.setBold(true);
    totalLabel_->setFont(totalFont);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(topItems_, QStringLiteral("Top 3 positions"));
    tabs->addTab(profiles_, QStringLiteral("Profiles"));
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(tree_);
    splitter->addWidget(tabs);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({420, 230});

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);
    layout->addWidget(fileLabel_);
    layout->addWidget(totalLabel_);
    layout->addWidget(splitter, 1);
    setCentralWidget(content);
}

void MainWindow::chooseCsv()
{
    const QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("Open BOM CSV"),
        currentFile_, QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (!filePath.isEmpty()) {
        loadCsv(filePath);
    }
}

void MainWindow::loadCsv(const QString& filePath)
{
    auto loaded = readBomCsv(filePath);
    if (const auto* error = std::get_if<Error>(&loaded)) {
        showError(*error, QStringLiteral("Could not open CSV"), QFileInfo(filePath).absoluteFilePath());
        return;
    }
    const auto applied = model_->setDocument(std::move(std::get<BomDocument>(loaded)));
    if (const auto* error = std::get_if<Error>(&applied)) {
        showError(*error, QStringLiteral("Could not open CSV"), QFileInfo(filePath).absoluteFilePath());
        return;
    }

    currentFile_ = QFileInfo(filePath).absoluteFilePath();
    fileLabel_->setText(currentFile_);
    fileLabel_->setToolTip(QStringLiteral("Source CSV. Cell edits are kept in memory; Save report exports the current analytics."));
    setWindowTitle(QStringLiteral("%1 - BOM Analyzer").arg(QFileInfo(filePath).fileName()));
    saveReportAction_->setEnabled(true);
    tree_->expandAll();
    statusBar()->showMessage(QStringLiteral("Loaded %1 building(s), %2 unique profile(s).")
        .arg(model_->rowCount()).arg(model_->analytics().profiles.size()));
}

void MainWindow::chooseReportPath()
{
    const QFileInfo source(currentFile_);
    QFileDialog dialog(this, QStringLiteral("Save BOM report"), source.absolutePath(),
        QStringLiteral("JSON files (*.json)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("json"));
    dialog.selectFile(source.completeBaseName() + QStringLiteral("-report.json"));
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
        saveReport(dialog.selectedFiles().front());
    }
}

void MainWindow::saveReport(const QString& filePath)
{
    if (!saveReportAction_->isEnabled()) {
        return;
    }
    const auto saved = writeAnalyticsJson(filePath, model_->analytics());
    if (const auto* error = std::get_if<Error>(&saved)) {
        showError(*error, QStringLiteral("Could not save report"), QFileInfo(filePath).absoluteFilePath());
        return;
    }
    statusBar()->showMessage(QStringLiteral("Report saved: %1").arg(QFileInfo(filePath).absoluteFilePath()));
}

void MainWindow::refreshAnalytics()
{
    const auto& report = model_->analytics();
    totalLabel_->setText(QStringLiteral("Overall cost: %1").arg(formatDecimal(report.overallCost)));
    totalLabel_->setToolTip(QStringLiteral("Overall cost: %1").arg(formatPreciseDecimal(report.overallCost)));
    expandAction_->setEnabled(model_->rowCount() > 0);
    collapseAction_->setEnabled(model_->rowCount() > 0);

    topItems_->setRowCount(static_cast<int>(report.topExpensiveItems.size()));
    for (int row = 0; row < topItems_->rowCount(); ++row) {
        const auto& item = report.topExpensiveItems[static_cast<std::size_t>(row)];
        setCell(topItems_, row, TopItemColumn::Building, item.buildingId);
        setCell(topItems_, row, TopItemColumn::Profile, item.profileName);
        setCell(topItems_, row, TopItemColumn::Position, QString::number(item.positionNumber));
        setCell(topItems_, row, TopItemColumn::TotalCost, item.totalCost);
    }

    profiles_->setRowCount(static_cast<int>(report.profiles.size()));
    for (int row = 0; row < profiles_->rowCount(); ++row) {
        const auto& profile = report.profiles[static_cast<std::size_t>(row)];
        setCell(profiles_, row, ProfileColumn::Name, profile.profileName);
        setCell(profiles_, row, ProfileColumn::TotalLength, profile.totalLengthMm / 1000.0);
        setCell(profiles_, row, ProfileColumn::TotalCost, profile.totalCost);
        setCell(profiles_, row, ProfileColumn::AverageCost, profile.averageCostPerMeter);
    }
}

void MainWindow::showError(const Error& error, const QString& title, const QString& details)
{
    QStringList context;
    if (error.line > 0) {
        context.push_back(QStringLiteral("Line: %1").arg(error.line));
    }
    if (!error.field.isEmpty()) {
        context.push_back(QStringLiteral("Field: %1").arg(error.field));
    }
    QMessageBox dialog(QMessageBox::Critical, title,
        error.message, QMessageBox::Ok, this);
    dialog.setTextFormat(Qt::PlainText);
    dialog.setInformativeText(context.join(u'\n'));
    dialog.setDetailedText(details);
    dialog.exec();
}

}
