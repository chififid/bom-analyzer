#include "io/bom_json_writer.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <limits>

class BomJsonWriterTest : public QObject
{
    Q_OBJECT
private slots:
    void reportRoundTripAndSafeReplacement()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("report-\u03a9.json"));
        const QString name = QStringLiteral("\u03a9\"profile\n");
        bom::AnalyticsResult report;
        report.overallCost = 1.0 / 3.0;
        report.boms = {{QStringLiteral("B2"), {report.overallCost, {}}}, {QStringLiteral("B1"), {0, {}}}};
        report.profiles = {{name, 2500.125, report.overallCost, 0.13332666699998333}};
        QVERIFY(std::holds_alternative<std::monostate>(bom::writeAnalyticsJson(path, report)));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        file.close();
        const QJsonObject expected{
            {"overallCost", report.overallCost},
            {"boms", QJsonArray{
                QJsonObject{{"buildingId", "B1"}, {"totalCost", 0}},
                QJsonObject{{"buildingId", "B2"}, {"totalCost", report.overallCost}}}},
            {"profiles", QJsonArray{QJsonObject{
                {"profileName", name}, {"totalLengthMm", 2500.125},
                {"totalCost", report.overallCost}, {"averageCostPerMeter", report.profiles[0].averageCostPerMeter}}}},
        };
        QCOMPARE(QJsonDocument::fromJson(original).object(), expected);

        report.profiles[0].averageCostPerMeter = std::numeric_limits<double>::infinity();
        const auto rejected = bom::writeAnalyticsJson(path, report);
        QVERIFY(std::holds_alternative<bom::Error>(rejected));
        QCOMPARE(std::get<bom::Error>(rejected).code, bom::ErrorCode::InvalidData);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
        file.close();

        QVERIFY(std::holds_alternative<std::monostate>(bom::writeAnalyticsJson(path, {})));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(),
            QJsonObject({{"overallCost", 0}, {"boms", QJsonArray{}}, {"profiles", QJsonArray{}}}));
    }

    void fileAccessFailures_data()
    {
        QTest::addColumn<QString>("relativePath");
        QTest::newRow("directory-as-file") << QStringLiteral(".");
        QTest::newRow("missing-parent") << QStringLiteral("missing/report.json");
    }

    void fileAccessFailures()
    {
        QFETCH(QString, relativePath);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto result = bom::writeAnalyticsJson(directory.filePath(relativePath), {});
        QVERIFY(std::holds_alternative<bom::Error>(result));
        QCOMPARE(std::get<bom::Error>(result).code, bom::ErrorCode::FileAccess);
    }
};

QTEST_APPLESS_MAIN(BomJsonWriterTest)
#include "bom_json_writer_test.moc"
