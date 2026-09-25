#include "profile_analyzer/profile_analysis.h"

#include <QtTest/QTest>

class ProfileAnalysisTest : public QObject
{
    Q_OBJECT
private slots:
    void analyzesNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("type");
        QTest::addColumn<double>("height");
        QTest::addColumn<double>("weight");
        QTest::newRow("ipe-default") << QStringLiteral("IPE200") << QStringLiteral("IPE") << 200.0 << 21.98;
        QTest::newRow("hea-default") << QStringLiteral("HEA160") << QStringLiteral("HEA") << 160.0 << 30.144;
        QTest::newRow("angle-explicit") << QStringLiteral("L150*5") << QStringLiteral("L") << 150.0 << 11.775;
        QTest::newRow("case-spaces-decimals-lower-x") << QStringLiteral(" \tipe 200 x 2.5\r\n") << QStringLiteral("IPE") << 200.0 << 7.85;
        QTest::newRow("upper-x") << QStringLiteral("HEA160X2") << QStringLiteral("HEA") << 160.0 << 7.536;
        QTest::newRow("multiplication-sign") << QStringLiteral("HEA160\u00d72") << QStringLiteral("HEA") << 160.0 << 7.536;
    }

    void analyzesNames()
    {
        QFETCH(QString, name);
        QFETCH(QString, type);
        QFETCH(double, height);
        QFETCH(double, weight);
        const auto result = profile_analyzer::analyzeProfile(name.toStdWString());
        const auto* info = std::get_if<profile_analyzer::ProfileInfo>(&result);
        QVERIFY(info);
        QCOMPARE(QString::fromStdWString(info->profileType), type);
        QCOMPARE(info->nominalHeight, height);
        QCOMPARE(info->weightPerMeter, weight);
    }

    void rejectsInvalidNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<profile_analyzer::Status>("code");
        const auto invalid = profile_analyzer::Status::InvalidFormat;
        const auto unsupported = profile_analyzer::Status::UnsupportedProfile;
        QTest::newRow("empty") << QString{} << invalid;
        QTest::newRow("missing-type") << QStringLiteral("200") << invalid;
        QTest::newRow("zero-height") << QStringLiteral("IPE0") << invalid;
        QTest::newRow("malformed-decimal") << QStringLiteral("IPE1.2.3") << invalid;
        QTest::newRow("missing-additional-size") << QStringLiteral("L150*") << invalid;
        QTest::newRow("extra-dimension") << QStringLiteral("L150*5*2") << invalid;
        QTest::newRow("invalid-separator") << QStringLiteral("IPE200/5") << invalid;
        QTest::newRow("type-too-long") << QString(static_cast<qsizetype>(profile_analyzer::profileTypeCapacity), u'A') + u'1' << invalid;
        QTest::newRow("number-out-of-range") << QStringLiteral("IPE") + QString(400, u'9') << invalid;
        QTest::newRow("weight-overflow") << QStringLiteral("L") + QString(200, u'9') + u'*' + QString(200, u'9') << invalid;
        QTest::newRow("unknown-type") << QStringLiteral("XYZ200") << unsupported;
        QTest::newRow("maximum-type-length") << QString(static_cast<qsizetype>(profile_analyzer::profileTypeCapacity - 1), u'A') + u'1' << unsupported;
    }

    void rejectsInvalidNames()
    {
        QFETCH(QString, name);
        QFETCH(profile_analyzer::Status, code);
        const auto result = profile_analyzer::analyzeProfile(name.toStdWString());
        const auto* error = std::get_if<profile_analyzer::Error>(&result);
        QVERIFY(error);
        QCOMPARE(error->code, code);
        QVERIFY(error->message && error->message[0] != '\0');
    }
};

QTEST_APPLESS_MAIN(ProfileAnalysisTest)
#include "profile_analysis_test.moc"
