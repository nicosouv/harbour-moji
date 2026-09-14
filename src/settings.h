#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

// Persisted preferences, exposed to QML as a context property.
//
// Thin on purpose: it stores strings and emits change signals. The meaning of a
// theme name belongs to qml/Mochi/Tokens.qml, and the meaning of a language code
// belongs to the OCR engine; this only remembers which ones were chosen.
class Settings : public QObject
{
    Q_OBJECT

    // The Mochi theme: "ambience", "mochi" or "mochiDark". "ambience" is the
    // default because an app that has not been told otherwise should look like it
    // belongs to the user's Sailfish, not to itself.
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)

    // Tesseract language codes to recognise with, e.g. ["fra", "eng"]. More than
    // one is normal and is the reason this is a list: a French document with
    // English words in it is the common case, not an edge case, and Tesseract
    // handles it by being initialised with "fra+eng".
    Q_PROPERTY(QStringList ocrLanguages READ ocrLanguages WRITE setOcrLanguages
                   NOTIFY ocrLanguagesChanged)

    // Joined with '+' the way Tesseract's Init() wants it. Read-only: it is
    // derived from ocrLanguages, and letting QML set it would give two spellings
    // of one fact.
    Q_PROPERTY(QString tesseractLanguages READ tesseractLanguages
                   NOTIFY ocrLanguagesChanged)

    // Straighten a photo taken sideways before recognising it, using osd. Costs a
    // pass over the image, so it is a choice rather than always-on.
    Q_PROPERTY(bool autoRotate READ autoRotate WRITE setAutoRotate
                   NOTIFY autoRotateChanged)

    // Threshold the photo against its local surroundings before recognising it.
    // On by default: Tesseract binarises internally with one threshold for the
    // whole page, which a hand-held photo's lighting gradient defeats. Worth
    // turning off only for a flat, evenly lit scan, where it can do nothing.
    Q_PROPERTY(bool enhanceContrast READ enhanceContrast WRITE setEnhanceContrast
                   NOTIFY enhanceContrastChanged)

    // Which languages are actually present, read from the tessdata directory
    // rather than from a list in the code.
    //
    // Tesseract ships 126 languages and all of them together are 339MB, so the
    // base package carries a few and the rest arrive as separate language-pack
    // RPMs. Scanning the directory means an installed pack simply appears in
    // Settings: no list to keep in step, and no version of the app that knows
    // about a pack it was built before.
    Q_PROPERTY(QStringList installedLanguages READ installedLanguages
                   NOTIFY installedLanguagesChanged)

public:
    explicit Settings(const QString &tessdataPath, QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);

    QStringList ocrLanguages() const;
    void setOcrLanguages(const QStringList &languages);

    QString tesseractLanguages() const;

    bool autoRotate() const;
    void setAutoRotate(bool enabled);

    bool enhanceContrast() const;
    void setEnhanceContrast(bool enabled);

    QStringList installedLanguages() const;

    // A readable name for a Tesseract language code, for the picker.
    //
    // Answered by src/languagenames.h, which is a table. QLocale was tried first
    // and cannot do it: QLocale("fra") resolves nothing, because Qt's table holds
    // ISO 639-1 and Tesseract's codes are ISO 639-2/T. Every language fell through
    // to the fallback and the picker listed the codes.
    Q_INVOKABLE QString languageName(const QString &code) const;

    // Re-read the tessdata directory. Worth calling when the app returns to the
    // foreground: a language pack can be installed while it is running.
    Q_INVOKABLE void refreshInstalledLanguages();

signals:
    void themeChanged();
    void ocrLanguagesChanged();
    void autoRotateChanged();
    void enhanceContrastChanged();
    void installedLanguagesChanged();

private:
    QSettings m_settings;
    QString m_tessdataPath;
    QStringList m_installedLanguages;
};

#endif // SETTINGS_H
