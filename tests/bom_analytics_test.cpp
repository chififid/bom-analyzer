#include "core/bom_analytics.h"
#include <QtTest/QTest>
#include <limits>
#include <utility>

namespace {
bom::BomDocument sampleDocument()
{
    return {{
        {QStringLiteral("B2"), {
            {QStringLiteral("IPE200"), 1000, 100, 2, bom::ItemId{41}},
            {QStringLiteral("ipe200"), 2000, 80, 1, bom::ItemId{7}},
        }},
        {QStringLiteral("B1"), {
            {QStringLiteral("IPE200"), 9000, 450, 1, bom::ItemId{93}},
            {QStringLiteral("Other"), 1000, 200, 1, bom::ItemId{12}},
        }},
    }};
}
}

class BomAnalyticsTest : public QObject
{
    Q_OBJECT
private slots:
    void formulasGroupingAndTopThree()
    {
        const auto calculated = bom::calculateAnalytics(sampleDocument());
        const auto* report = std::get_if<bom::AnalyticsResult>(&calculated);
        QVERIFY(report);
        QCOMPARE(report->overallCost, 930.0);
        QCOMPARE(report->boms.value(QStringLiteral("B2")).totalCost, 280.0);
        QCOMPARE(report->boms.value(QStringLiteral("B1")).totalCost, 650.0);
        const auto item = report->boms.value(QStringLiteral("B2")).itemMetrics.value(bom::ItemId{41});
        QCOMPARE(item.totalCost, 200.0);
        QCOMPARE(item.totalLengthMm, 2000.0);
        QCOMPARE(item.costPerMeter, 100.0);
        QCOMPARE(report->profiles.size(), std::size_t{3});
        const auto& profile = report->profiles[0];
        QCOMPARE(profile.profileName, QStringLiteral("IPE200"));
        QCOMPARE(profile.totalCost, 650.0);
        QCOMPARE(profile.totalLengthMm, 11000.0);
        QCOMPARE(profile.averageCostPerMeter, 650.0 / 11.0);
        QCOMPARE(report->profiles[1].profileName, QStringLiteral("ipe200"));
        QCOMPARE(report->profiles[1].totalCost, 80.0);
        const auto& top = report->topExpensiveItems;
        QCOMPARE(top.size(), std::size_t{3});
        QCOMPARE(top[0].itemId, bom::ItemId{93});
        QCOMPARE(top[0].totalCost, 450.0);
        QCOMPARE(top[1].itemId, bom::ItemId{41});
        QCOMPARE(top[2].itemId, bom::ItemId{12});
        QCOMPARE(top[1].buildingId, QStringLiteral("B2"));
        QCOMPARE(top[1].positionNumber, std::size_t{1});
    }

    void identitySurvivesReorderingAndReportsAreIndependent()
    {
        auto document = sampleDocument();
        document.boms[0].items[1].profileName = document.boms[0].items[0].profileName;
        const auto calculated = bom::calculateAnalytics(document);
        const auto* report = std::get_if<bom::AnalyticsResult>(&calculated);
        QVERIFY(report);
        // Change source order independently of the existing report.
        std::swap(document.boms[0].items[0], document.boms[0].items[1]);
        std::swap(document.boms[0], document.boms[1]);
        auto& building = document.boms[1];
        const auto summary = report->boms.value(building.buildingId);
        QCOMPARE(summary.itemMetrics.value(building.items[0].id).totalCost, 80.0);
        QCOMPARE(summary.itemMetrics.value(building.items[1].id).totalCost, 200.0);
        building.items[1].cost = 150;
        const auto recalculated = bom::calculateAnalytics(document);
        const auto* updated = std::get_if<bom::AnalyticsResult>(&recalculated);
        QVERIFY(updated);
        QCOMPARE(updated->overallCost, 1030.0);
        QCOMPARE(updated->boms.value(building.buildingId).itemMetrics.value(bom::ItemId{41}).totalCost, 300.0);
        QCOMPARE(report->overallCost, 930.0);
        QCOMPARE(report->boms.value(building.buildingId).itemMetrics.value(bom::ItemId{41}).totalCost, 200.0);
    }

    void emptyAndSmallDocuments_data()
    {
        QTest::addColumn<bom::BomDocument>("document");
        QTest::addColumn<int>("profileCount");
        QTest::addColumn<int>("topCount");
        QTest::addColumn<double>("cost");
        const bom::ProfileItem free{QStringLiteral("Free"), 500, 0, 3, bom::ItemId{1}};
        const bom::ProfileItem paid{QStringLiteral("Paid"), 500, 12.5, 2, bom::ItemId{2}};
        QTest::newRow("empty-document") << bom::BomDocument{} << 0 << 0 << 0.0;
        QTest::newRow("empty-building")
            << bom::BomDocument{{{QStringLiteral("B1"), {}}}} << 0 << 0 << 0.0;
        QTest::newRow("free-position")
            << bom::BomDocument{{{QStringLiteral("B1"), {free}}}} << 1 << 1 << 0.0;
        QTest::newRow("two-positions")
            << bom::BomDocument{{{QStringLiteral("B1"), {free, paid}}}} << 2 << 2 << 25.0;
    }

    void emptyAndSmallDocuments()
    {
        QFETCH(bom::BomDocument, document);
        QFETCH(int, profileCount);
        QFETCH(int, topCount);
        QFETCH(double, cost);
        const auto calculated = bom::calculateAnalytics(document);
        const auto* report = std::get_if<bom::AnalyticsResult>(&calculated);
        QVERIFY(report);
        QCOMPARE(report->overallCost, cost);
        QCOMPARE(report->boms.size(), static_cast<qsizetype>(document.boms.size()));
        QCOMPARE(report->profiles.size(), static_cast<std::size_t>(profileCount));
        QCOMPARE(report->topExpensiveItems.size(), static_cast<std::size_t>(topCount));
    }

    void invalidDocuments_data()
    {
        QTest::addColumn<bom::BomDocument>("document");
        QTest::addColumn<bom::ErrorCode>("code");
        QTest::addColumn<QString>("field");
        const auto invalid = bom::ErrorCode::InvalidData;
        const auto row = [invalid](const char* name, bom::ProfileItem item, const QString& field) {
            item.id = bom::ItemId{1};
            QTest::newRow(name) << bom::BomDocument{{{QStringLiteral("B1"), {item}}}} << invalid << field;
        };
        const auto name = QStringLiteral("A");
        row("blank-name", {QStringLiteral(" \t"), 1000, 10, 1}, QStringLiteral("ProfileName"));
        row("zero-length", {name, 0, 10, 1}, QStringLiteral("Length_mm"));
        row("infinite-length", {name, std::numeric_limits<double>::infinity(), 10, 1}, QStringLiteral("Length_mm"));
        row("negative-cost", {name, 1000, -1, 1}, QStringLiteral("Cost"));
        row("nan-cost", {name, 1000, std::numeric_limits<double>::quiet_NaN(), 1}, QStringLiteral("Cost"));
        row("zero-quantity", {name, 1000, 10, 0}, QStringLiteral("Quantity"));
        QTest::newRow("blank-building") << bom::BomDocument{{{QStringLiteral(" "), {}}}}
            << invalid << QStringLiteral("BuildingID");
        QTest::newRow("duplicate-building") << bom::BomDocument{{{name, {}}, {name, {}}}}
            << invalid << QStringLiteral("BuildingID");
        auto document = sampleDocument();
        document.boms[0].items[0].id = bom::ItemId::Invalid;
        QTest::newRow("missing-id") << document << invalid << QStringLiteral("ItemID");
        document = sampleDocument();
        document.boms[0].items[1].id = document.boms[0].items[0].id;
        QTest::newRow("duplicate-within-building") << document << invalid << QStringLiteral("ItemID");
        document = sampleDocument();
        document.boms[1].items[0].id = document.boms[0].items[0].id;
        QTest::newRow("duplicate-across-buildings") << document << invalid << QStringLiteral("ItemID");
        const double maximum = std::numeric_limits<double>::max();
        const bom::ProfileItem costly{name, 1000, maximum * 0.4, 1};
        const bom::ProfileItem longItem{name, maximum * 0.4, 0, 1};
        const auto overflow = [](const char* label, bom::BomDocument document) {
            quint64 id = 1;
            for (auto& building : document.boms) {
                for (auto& item : building.items) {
                    item.id = bom::ItemId{id++};
                }
            }
            QTest::newRow(label) << document << bom::ErrorCode::NumericOverflow << QString{};
        };
        overflow("item-cost", {{{name, {{name, 1000, maximum, 2}}}}});
        overflow("item-length", {{{name, {{name, maximum, 10, 2}}}}});
        overflow("cost-per-metre", {{{name, {{name, 1, maximum, 1}}}}});
        overflow("bom-cost", {{{name, {costly, costly, costly}}}});
        overflow("profile-cost", {{{name, {costly}}, {QStringLiteral("B"), {costly}}, {QStringLiteral("C"), {costly}}}});
        overflow("overall-cost", {{{name, {costly}},
            {QStringLiteral("B"), {{QStringLiteral("B"), 1000, maximum * 0.4, 1}}},
            {QStringLiteral("C"), {{QStringLiteral("C"), 1000, maximum * 0.4, 1}}}}});
        overflow("profile-length", {{{name, {longItem, longItem, longItem}}}});
    }

    void invalidDocuments()
    {
        QFETCH(bom::BomDocument, document);
        QFETCH(bom::ErrorCode, code);
        QFETCH(QString, field);
        const auto calculated = bom::calculateAnalytics(document);
        const auto* error = std::get_if<bom::Error>(&calculated);
        QVERIFY(error);
        QCOMPARE(error->code, code);
        if (!field.isEmpty()) {
            QCOMPARE(error->field, field);
        }
        QCOMPARE(error->line, qsizetype{0});
        QVERIFY(!error->message.isEmpty());
    }
};

QTEST_APPLESS_MAIN(BomAnalyticsTest)
#include "bom_analytics_test.moc"
