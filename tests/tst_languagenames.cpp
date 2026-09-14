#include <QtTest>

#include "languagenames.h"

// Naming a Tesseract language code.
//
// The layer exists because QLocale cannot do this, and the test that matters most
// is the one that pins exactly that: QLocale("fra") resolves to nothing, so the
// picker used to list "fra". Every assertion here is about a name a user reads.
class TestLanguageNames : public QObject
{
    Q_OBJECT

private slots:
    void qlocaleReallyCannotDoThis();
    void namesTheCodesTheUserComplainedAbout();
    void saysOneNameWhenBothAreTheSame();
    void namesTheVariantsToo();
    void unknownCodeComesBackAsItself();
    void codeCaseAndSpaceDoNotMatter();
    void osdAndEquAreNotLanguages();
    void defaultFollowsThePhone();
    void defaultOnlyOffersWhatIsInstalled();
};

void TestLanguageNames::qlocaleReallyCannotDoThis()
{
    // Not a test of our code - a test of the premise it was written on. If a
    // future Qt learns ISO 639-2/T this fails, and the table could be dropped.
    QCOMPARE(QLocale(QStringLiteral("fra")).language(), QLocale::C);
    QCOMPARE(QLocale(QStringLiteral("ces")).language(), QLocale::C);
    QCOMPARE(QLocale(QStringLiteral("ell")).language(), QLocale::C);
}

void TestLanguageNames::namesTheCodesTheUserComplainedAbout()
{
    QCOMPARE(LanguageNames::displayName(QStringLiteral("fra")),
             QStringLiteral("Français (French)"));
    QCOMPARE(LanguageNames::displayName(QStringLiteral("ces")),
             QStringLiteral("Čeština (Czech)"));
    QCOMPARE(LanguageNames::displayName(QStringLiteral("ell")),
             QStringLiteral("Ελληνικά (Greek)"));
    QCOMPARE(LanguageNames::displayName(QStringLiteral("chi_sim")),
             QString::fromUtf8("简体中文 (Chinese, Simplified)"));
}

void TestLanguageNames::saysOneNameWhenBothAreTheSame()
{
    // "English (English)" and "Esperanto (Esperanto)" are noise. The gloss is
    // there for "Suomi", which an English or French reader would not place.
    QCOMPARE(LanguageNames::displayName(QStringLiteral("eng")),
             QStringLiteral("English"));
    QCOMPARE(LanguageNames::displayName(QStringLiteral("epo")),
             QStringLiteral("Esperanto"));
    QCOMPARE(LanguageNames::displayName(QStringLiteral("fin")),
             QStringLiteral("Suomi (Finnish)"));
}

void TestLanguageNames::namesTheVariantsToo()
{
    // The qualified codes are the ones the old QLocale path mangled worst: it
    // produced "Chinese (sim)" by splitting on the underscore.
    QCOMPARE(LanguageNames::endonym(QStringLiteral("srp_latn")),
             QStringLiteral("Srpski"));
    QCOMPARE(LanguageNames::englishName(QStringLiteral("srp_latn")),
             QStringLiteral("Serbian, Latin"));
    QCOMPARE(LanguageNames::englishName(QStringLiteral("jpn_vert")),
             QStringLiteral("Japanese, vertical"));
    QCOMPARE(LanguageNames::englishName(QStringLiteral("deu_latf")),
             QStringLiteral("German, Fraktur"));
}

void TestLanguageNames::unknownCodeComesBackAsItself()
{
    // A language pack built from a Tesseract newer than this table. A raw code is
    // honest; a guessed name would not be.
    QCOMPARE(LanguageNames::displayName(QStringLiteral("xyz")),
             QStringLiteral("xyz"));
    QVERIFY(LanguageNames::endonym(QStringLiteral("xyz")).isEmpty());
    QVERIFY(LanguageNames::englishName(QStringLiteral("xyz")).isEmpty());
}

void TestLanguageNames::codeCaseAndSpaceDoNotMatter()
{
    QCOMPARE(LanguageNames::displayName(QStringLiteral("  FRA ")),
             QStringLiteral("Français (French)"));
}

void TestLanguageNames::osdAndEquAreNotLanguages()
{
    QVERIFY(!LanguageNames::isReadable(QStringLiteral("osd")));
    QVERIFY(!LanguageNames::isReadable(QStringLiteral("equ")));
    QVERIFY(LanguageNames::isReadable(QStringLiteral("fra")));
    QVERIFY(LanguageNames::isReadable(QStringLiteral("chi_sim")));
}

void TestLanguageNames::defaultFollowsThePhone()
{
    const QStringList installed { QStringLiteral("eng"), QStringLiteral("fra"),
                                  QStringLiteral("deu"), QStringLiteral("chi_sim"),
                                  QStringLiteral("nor") };

    QCOMPARE(LanguageNames::codeForLocale(QLocale(QStringLiteral("fr_FR")), installed),
             QStringLiteral("fra"));
    QCOMPARE(LanguageNames::codeForLocale(QLocale(QStringLiteral("de_CH")), installed),
             QStringLiteral("deu"));
    QCOMPARE(LanguageNames::codeForLocale(QLocale(QStringLiteral("zh_CN")), installed),
             QStringLiteral("chi_sim"));

    // A Norwegian phone reports Bokmal or Nynorsk, never plain "no", and
    // Tesseract has one Norwegian model for both.
    QCOMPARE(LanguageNames::codeForLocale(QLocale(QLocale::NorwegianBokmal,
                                                  QLocale::Norway), installed),
             QStringLiteral("nor"));
}

void TestLanguageNames::defaultOnlyOffersWhatIsInstalled()
{
    // Portuguese is a language the table knows and this phone does not have, so
    // the caller has to be told nothing fits rather than handed a code whose
    // language pack was never installed.
    const QStringList installed { QStringLiteral("eng"), QStringLiteral("fra") };
    QVERIFY(LanguageNames::codeForLocale(QLocale(QStringLiteral("pt_BR")),
                                         installed).isEmpty());
    QCOMPARE(LanguageNames::codeForLocale(QLocale(QStringLiteral("fr_BE")), installed),
             QStringLiteral("fra"));
}

QTEST_APPLESS_MAIN(TestLanguageNames)
#include "tst_languagenames.moc"
