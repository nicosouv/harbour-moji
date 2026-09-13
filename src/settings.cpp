#include "settings.h"

#include "logging.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

namespace {

const char *kThemeKey = "theme";
const char *kLanguagesKey = "ocrLanguages";
const char *kAutoRotateKey = "autoRotate";
const char *kEnhanceKey = "enhanceContrast";

// Only the themes Tokens.qml actually implements. A stored value from a newer
// version, or a hand-edited config, falls back rather than leaving the UI reading
// colours that do not exist.
const QStringList &knownThemes()
{
    static const QStringList themes { QStringLiteral("ambience"),
                                      QStringLiteral("mochi"),
                                      QStringLiteral("mochiDark") };
    return themes;
}

} // namespace

Settings::Settings(const QString &tessdataPath, QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("harbour-moji"), QStringLiteral("harbour-moji"))
    , m_tessdataPath(tessdataPath)
{
    refreshInstalledLanguages();
}

QString Settings::theme() const
{
    const QString stored = m_settings.value(QLatin1String(kThemeKey),
                                            QStringLiteral("ambience")).toString();
    return knownThemes().contains(stored) ? stored : QStringLiteral("ambience");
}

void Settings::setTheme(const QString &theme)
{
    if (!knownThemes().contains(theme)) {
        qCWarning(lcMoji) << "ignoring unknown theme" << theme;
        return;
    }
    if (theme == this->theme()) {
        return;
    }
    m_settings.setValue(QLatin1String(kThemeKey), theme);
    emit themeChanged();
}

QStringList Settings::ocrLanguages() const
{
    const QStringList stored =
        m_settings.value(QLatin1String(kLanguagesKey)).toStringList();
    if (!stored.isEmpty()) {
        return stored;
    }

    // Never chosen, so guess from the phone's own language: someone reading a
    // French page on a French phone should not have to find a setting first, and
    // defaulting to English made every accented word a small error.
    //
    // Matched through QLocale rather than a table of codes - the same trick
    // languageName() uses - so this keeps working for any language pack installed
    // later without being taught about it.
    const QLocale::Language systemLanguage = QLocale::system().language();
    for (const QString &code : m_installedLanguages) {
        if (QLocale(code).language() == systemLanguage) {
            return QStringList { code };
        }
    }

    return QStringList { QStringLiteral("eng") };
}

void Settings::setOcrLanguages(const QStringList &languages)
{
    if (languages.isEmpty() || languages == ocrLanguages()) {
        return;
    }
    m_settings.setValue(QLatin1String(kLanguagesKey), languages);
    emit ocrLanguagesChanged();
}

QString Settings::tesseractLanguages() const
{
    return ocrLanguages().join(QLatin1Char('+'));
}

bool Settings::autoRotate() const
{
    return m_settings.value(QLatin1String(kAutoRotateKey), true).toBool();
}

void Settings::setAutoRotate(bool enabled)
{
    if (enabled == autoRotate()) {
        return;
    }
    m_settings.setValue(QLatin1String(kAutoRotateKey), enabled);
    emit autoRotateChanged();
}

QStringList Settings::installedLanguages() const
{
    return m_installedLanguages;
}

void Settings::refreshInstalledLanguages()
{
    QStringList found;

    const QDir dir(m_tessdataPath);
    const QStringList files = dir.entryList(QStringList { QStringLiteral("*.traineddata") },
                                            QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString code = QFileInfo(file).completeBaseName();
        // osd is orientation and script detection, not a language anyone reads
        // in. Offering it as a choice would be offering to recognise text with a
        // model that recognises no text.
        if (code == QLatin1String("osd")) {
            continue;
        }
        found.append(code);
    }

    if (found == m_installedLanguages) {
        return;
    }

    m_installedLanguages = found;
    qCDebug(lcMoji) << "language data present:" << found;
    emit installedLanguagesChanged();
}

QString Settings::languageName(const QString &code) const
{
    // Exact code first: QLocale understands the three-letter ISO 639-2 codes
    // Tesseract uses for most of its languages.
    QLocale locale(code);
    if (locale.language() != QLocale::C) {
        return locale.nativeLanguageName();
    }

    // Then the part before the qualifier, which is what "chi_sim", "chi_tra" and
    // the "_vert" variants are: a language QLocale knows, plus a script or writing
    // direction it does not express this way.
    const int underscore = code.indexOf(QLatin1Char('_'));
    if (underscore > 0) {
        QLocale base(code.left(underscore));
        if (base.language() != QLocale::C) {
            return base.nativeLanguageName() + QStringLiteral(" (")
                   + code.mid(underscore + 1) + QLatin1Char(')');
        }
    }

    // Neither: show the code. A wrong name would be worse than a raw one.
    return code;
}

bool Settings::enhanceContrast() const
{
    return m_settings.value(QLatin1String(kEnhanceKey), true).toBool();
}

void Settings::setEnhanceContrast(bool enabled)
{
    if (enabled == enhanceContrast()) {
        return;
    }
    m_settings.setValue(QLatin1String(kEnhanceKey), enabled);
    emit enhanceContrastChanged();
}
