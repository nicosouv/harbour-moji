#include "settings.h"

#include "languagenames.h"
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
    // defaulting to English makes every accented word a small error.
    //
    // This used to compare QLocale(code).language() against the system's, which
    // never matched anything: QLocale("fra") is QLocale::C, so a French phone
    // quietly read French pages as English. LanguageNames does the matching on the
    // ISO 639-1 code instead, which is the half QLocale actually speaks.
    const QString guess = LanguageNames::codeForLocale(QLocale::system(),
                                                       m_installedLanguages);
    if (!guess.isEmpty()) {
        return QStringList { guess };
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
        // osd detects orientation and script, equ detects mathematics. Neither
        // reads text, so offering either as a language would be offering to
        // recognise text with a model that recognises none.
        if (!LanguageNames::isReadable(code)) {
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
    return LanguageNames::displayName(code);
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
