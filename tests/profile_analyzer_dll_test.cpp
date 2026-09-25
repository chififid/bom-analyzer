#include "profile_analyzer/profile_analyzer_api.h"

#include <QCoreApplication>
#include <QtTest/QTest>

#include <windows.h>

#include <array>

class ProfileAnalyzerDllTest : public QObject
{
    Q_OBJECT

    enum class MissingArgument { None, Name, Height, Weight, Type };

    HMODULE library_{};
    decltype(&AnalyzeProfile) analyze_{};

private slots:
    void initTestCase()
    {
        const auto path = QCoreApplication::applicationDirPath()
            + QStringLiteral("/ProfileAnalyzer.dll");
        library_ = LoadLibraryW(path.toStdWString().c_str());
        QVERIFY2(library_, qPrintable(QStringLiteral("LoadLibraryW failed: %1").arg(GetLastError())));
        analyze_ = reinterpret_cast<decltype(analyze_)>(GetProcAddress(library_, "AnalyzeProfile"));
        QVERIFY2(analyze_, "The AnalyzeProfile export was not found.");
    }

    void cleanupTestCase()
    {
        if (library_) {
            QVERIFY(FreeLibrary(library_));
        }
    }

    void returnsStatusAndOutputs_data()
    {
        using profile_analyzer::Status;
        QTest::addColumn<QString>("name");
        QTest::addColumn<MissingArgument>("missing");
        QTest::addColumn<Status>("status");
        QTest::newRow("success") << QStringLiteral("IPE200") << MissingArgument::None << Status::Success;
        QTest::newRow("invalid-format") << QStringLiteral("IPE0") << MissingArgument::None << Status::InvalidFormat;
        QTest::newRow("unsupported-type") << QStringLiteral("XYZ200") << MissingArgument::None << Status::UnsupportedProfile;
        QTest::newRow("null-name") << QStringLiteral("IPE200") << MissingArgument::Name << Status::InvalidArgument;
        QTest::newRow("null-height") << QStringLiteral("IPE200") << MissingArgument::Height << Status::InvalidArgument;
        QTest::newRow("null-weight") << QStringLiteral("IPE200") << MissingArgument::Weight << Status::InvalidArgument;
        QTest::newRow("null-type") << QStringLiteral("IPE200") << MissingArgument::Type << Status::InvalidArgument;
    }

    void returnsStatusAndOutputs()
    {
        QFETCH(QString, name);
        QFETCH(MissingArgument, missing);
        QFETCH(profile_analyzer::Status, status);
        const auto wideName = name.toStdWString();
        double height = -1;
        double weight = -1;
        std::array<wchar_t, profile_analyzer::profileTypeCapacity + 1> type;
        type.fill(L'?');

        const int result = analyze_(missing == MissingArgument::Name ? nullptr : wideName.c_str(),
            missing == MissingArgument::Height ? nullptr : &height,
            missing == MissingArgument::Weight ? nullptr : &weight,
            missing == MissingArgument::Type ? nullptr : type.data());

        QCOMPARE(result, static_cast<int>(status));
        if (status == profile_analyzer::Status::Success) {
            QCOMPARE(height, 200.0);
            QCOMPARE(weight, 21.98);
            QCOMPARE(QString::fromWCharArray(type.data(), 3), QStringLiteral("IPE"));
            QCOMPARE(type[3], L'\0');
        } else {
            QCOMPARE(height, missing == MissingArgument::Height ? -1.0 : 0.0);
            QCOMPARE(weight, missing == MissingArgument::Weight ? -1.0 : 0.0);
            QCOMPARE(type[0], missing == MissingArgument::Type ? L'?' : L'\0');
        }
        QCOMPARE(type.back(), L'?');
    }
};

QTEST_GUILESS_MAIN(ProfileAnalyzerDllTest)
#include "profile_analyzer_dll_test.moc"
