#include "io/bom_csv_reader.h"
#include "core/bom_analytics.h"
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <limits>

namespace {
const QByteArray header = "BuildingID,ProfileName,Length_mm,Cost,Quantity\n";
}

class BomCsvReaderTest : public QObject
{
    Q_OBJECT
private slots:
    void sampleFileProducesExpectedReport()
    {
        const auto loaded = bom::readBomCsv(QFINDTESTDATA("data/bom_sample.csv"));
        const auto* document = std::get_if<bom::BomDocument>(&loaded);
        QVERIFY(document);
        QCOMPARE(document->boms.size(), std::size_t{2});
        QCOMPARE(document->boms[0].items.size(), std::size_t{2});
        const auto calculated = bom::calculateAnalytics(*document);
        const auto* report = std::get_if<bom::AnalyticsResult>(&calculated);
        QVERIFY(report);
        QCOMPARE(report->boms.value(QStringLiteral("B0001")).totalCost, 1080.0);
        QCOMPARE(report->boms.value(QStringLiteral("B0002")).totalCost, 480.0);
        QCOMPARE(report->overallCost, 1560.0);
    }

    void groupingPreservesOrderExactNamesAndDuplicatePositions()
    {
        const auto loaded = bom::parseBomCsv(header
            + "B2,IPE200,1000,10,1\nB1,A,1000,20,1\nB2,IPE200,1000,10,1\n"
              "b2,ipe200,1000,10,1\n B2 , IPE200 ,1000,10,1\n");
        const auto* document = std::get_if<bom::BomDocument>(&loaded);
        QVERIFY(document);
        QStringList buildings;
        QSet<bom::ItemId> ids;
        for (const auto& building : document->boms) {
            buildings.push_back(building.buildingId);
            for (const auto& item : building.items) {
                QVERIFY(item.id != bom::ItemId::Invalid && !ids.contains(item.id));
                ids.insert(item.id);
            }
        }
        QCOMPARE(buildings, QStringList({"B2", "B1", "b2", " B2 "}));
        QCOMPARE(ids.size(), qsizetype{5});
        QCOMPARE(document->boms[0].items.size(), std::size_t{2});
        QCOMPARE(document->boms[0].items[1].profileName, QStringLiteral("IPE200"));
        QCOMPARE(document->boms[3].items.size(), std::size_t{1});
        QCOMPARE(document->boms[3].items[0].profileName, QStringLiteral(" IPE200 "));
    }

    void quotedUnicodeAndNumbersIgnoreTheDefaultLocale()
    {
        const QByteArray data = QByteArray::fromHex("efbbbf") + QStringLiteral(
            "\"BuildingID\",\"ProfileName\",\"Length_mm\",\"Cost\",\"Quantity\"\r\n"
            "\"Building, 1\",\"Profile \"\"\u03a9\"\"\r\n200\",\" 1.5e3 \",\" 12.5 \",\" 2 \"\r\n"
            "B2,Free,1000,0,").toUtf8() + QByteArray::number(std::numeric_limits<int>::max()) + "\r\n";
        const QLocale previous;
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
        const auto loaded = bom::parseBomCsv(data);
        QLocale::setDefault(previous);
        const auto* document = std::get_if<bom::BomDocument>(&loaded);
        QVERIFY(document);
        QCOMPARE(document->boms.size(), std::size_t{2});
        QCOMPARE(document->boms[0].buildingId, QStringLiteral("Building, 1"));
        QCOMPARE(document->boms[0].items.size(), std::size_t{1});
        QCOMPARE(document->boms[1].items.size(), std::size_t{1});
        const auto& item = document->boms[0].items[0];
        QCOMPARE(item.profileName, QStringLiteral("Profile \"\u03a9\"\n200"));
        QCOMPARE(item.lengthMm, 1500.0);
        QCOMPARE(item.cost, 12.5);
        QCOMPARE(item.quantity, 2);
        QCOMPARE(document->boms[1].items[0].quantity, std::numeric_limits<int>::max());
        QCOMPARE(document->boms[1].items[0].cost, 0.0);
        const auto invalid = bom::parseBomCsv(data + "B2,A,0,10,1\n");
        const auto* error = std::get_if<bom::Error>(&invalid);
        QVERIFY(error);
        QCOMPARE(error->line, qsizetype{5});
        QCOMPARE(error->field, QStringLiteral("Length_mm"));
    }

    void recordEndingsAndHeaderOnly_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<int>("buildingCount");
        QTest::newRow("lf") << header + "B1,A,1000,10,1\n" << 1;
        QTest::newRow("crlf") << header.trimmed() + "\r\nB1,A,1000,10,1\r\n" << 1;
        QTest::newRow("cr") << header.trimmed() + "\rB1,A,1000,10,1\r" << 1;
        QTest::newRow("no-final-newline") << header + "B1,A,1000,10,1" << 1;
        QTest::newRow("header-only") << header.trimmed() << 0;
    }

    void recordEndingsAndHeaderOnly()
    {
        QFETCH(QByteArray, data);
        QFETCH(int, buildingCount);
        const auto loaded = bom::parseBomCsv(data);
        const auto* document = std::get_if<bom::BomDocument>(&loaded);
        QVERIFY(document);
        QCOMPARE(document->boms.size(), static_cast<std::size_t>(buildingCount));
        if (buildingCount > 0) {
            QCOMPARE(document->boms[0].items.size(), std::size_t{1});
        }
    }

    void invalidInput_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bom::ErrorCode>("code");
        QTest::addColumn<qsizetype>("line");
        QTest::addColumn<QString>("field");
        using enum bom::ErrorCode;
        QTest::newRow("empty-file") << QByteArray{} << InvalidCsv << qsizetype{1} << QString{};
        QTest::newRow("wrong-header") << QByteArray("ProfileName,BuildingID,Length_mm,Cost,Quantity\n")
            << InvalidCsv << qsizetype{1} << QString{};
        const auto row = [](const char* name, const QByteArray& record,
                            bom::ErrorCode code, const QString& field = {}) {
            QTest::newRow(name) << header + record << code << qsizetype{2} << field;
        };
        row("wrong-field-count", "B1,A,1000,10", InvalidCsv);
        row("blank-building", " \t,A,1000,10,1", InvalidData, QStringLiteral("BuildingID"));
        row("blank-profile", "B1,\" \t\",1000,10,1", InvalidData, QStringLiteral("ProfileName"));
        row("text-length", "B1,A,long,10,1", InvalidData, QStringLiteral("Length_mm"));
        row("zero-length", "B1,A,0,10,1", InvalidData, QStringLiteral("Length_mm"));
        row("infinite-length", "B1,A,inf,10,1", InvalidData, QStringLiteral("Length_mm"));
        row("overflow-length", "B1,A,1e309,10,1", InvalidData, QStringLiteral("Length_mm"));
        row("negative-cost", "B1,A,1000,-1,1", InvalidData, QStringLiteral("Cost"));
        row("nan-cost", "B1,A,1000,nan,1", InvalidData, QStringLiteral("Cost"));
        row("fractional-quantity", "B1,A,1000,10,1.5", InvalidData, QStringLiteral("Quantity"));
        row("overflow-quantity", "B1,A,1000,10,999999999999", InvalidData, QStringLiteral("Quantity"));
        row("quote-in-field", "B1,IP\"E,1000,10,1", InvalidCsv, QStringLiteral("ProfileName"));
        row("unclosed-quote", "B1,\"IPE,1000,10,1\n", InvalidCsv, QStringLiteral("ProfileName"));
        row("text-after-quote", "B1,\"IPE\"x,1000,10,1", InvalidCsv, QStringLiteral("ProfileName"));
        QTest::newRow("invalid-utf8") << header + QByteArray::fromHex("ff")
            << InvalidEncoding << qsizetype{0} << QString{};
        QTest::newRow("incomplete-utf8") << header + QByteArray::fromHex("d0")
            << InvalidEncoding << qsizetype{0} << QString{};
        QTest::newRow("nul-byte") << header + QByteArray::fromHex("00")
            << InvalidEncoding << qsizetype{0} << QString{};
    }

    void invalidInput()
    {
        QFETCH(QByteArray, data);
        QFETCH(bom::ErrorCode, code);
        QFETCH(qsizetype, line);
        QFETCH(QString, field);
        const auto loaded = bom::parseBomCsv(data);
        const auto* error = std::get_if<bom::Error>(&loaded);
        QVERIFY(error);
        QCOMPARE(error->code, code);
        QCOMPARE(error->line, line);
        QCOMPARE(error->field, field);
        QVERIFY(!error->message.isEmpty());
    }

    void fileAccessAndUnicodePath()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("BOM-\u03a9.csv"));
        const auto missing = bom::readBomCsv(path);
        QVERIFY(std::holds_alternative<bom::Error>(missing));
        QCOMPARE(std::get<bom::Error>(missing).code, bom::ErrorCode::FileAccess);
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(header), static_cast<qint64>(header.size()));
        file.close();
        QVERIFY(std::holds_alternative<bom::BomDocument>(bom::readBomCsv(path)));
    }
};

QTEST_APPLESS_MAIN(BomCsvReaderTest)
#include "bom_csv_reader_test.moc"
