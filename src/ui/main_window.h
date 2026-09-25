#pragma once

#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QTableWidget;
class QTreeView;

namespace bom {

class BomTreeModel;
struct Error;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void loadCsv(const QString& filePath);
    void saveReport(const QString& filePath);

private:
    void setupTree();
    void setupActionsAndMenus();
    void setupLayout();
    void chooseCsv();
    void chooseReportPath();
    void showTreeContextMenu(const QPoint& position);
    void analyzeCurrentProfile();
    void refreshProfileAction();
    void refreshAnalytics();
    void showError(const Error& error, const QString& title, const QString& details = {});

    BomTreeModel* model_;
    QTreeView* tree_;
    QLabel* fileLabel_;
    QLabel* totalLabel_;
    QTableWidget* topItems_;
    QTableWidget* profiles_;
    QAction* expandAction_;
    QAction* collapseAction_;
    QAction* saveReportAction_;
    QAction* analyzeProfileAction_;
    QString currentFile_;
};

}
