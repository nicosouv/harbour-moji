#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QFutureWatcher>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <QStringList>
#include <QVariantList>

#include "ocrresult.h"
#include "textlayout.h"

namespace tesseract {
class TessBaseAPI;
}

// The only file that talks to Tesseract, and a translation layer rather than a
// decision-making one: it turns a QImage into an OcrResult and gets out of the
// way. What that result means - which word a tap hit, how far a fingertip may
// miss by, whether a number checks out - belongs to textlayout and fieldparser,
// which is why those can be tested and this cannot.
//
// Recognition runs on a worker thread. On a phone a full page is seconds, not
// milliseconds, and doing it on the GUI thread would freeze the scroll of the
// very image being read.
class OcrEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString text READ text NOTIFY resultChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY resultChanged)
    Q_PROPERTY(qreal confidence READ confidence NOTIFY resultChanged)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY resultChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

    // The structured things found in the text: IBANs, card numbers, ISBNs, a
    // passport's machine-readable zone, emails, links. Each carries whether its
    // own checksum agreed, which is the part worth showing - a photographed IBAN
    // that fails mod-97 means "read this line again", and saying so is more use
    // than copying the wrong digits confidently.
    Q_PROPERTY(QVariantList fields READ fields NOTIFY resultChanged)

    // One entry per recognised line: its box in the original photo's coordinates,
    // and its mean confidence. The UI draws these faintly over the photo, which
    // is what tells the user there is text there to tap before they have tapped
    // anything - and, tinted by confidence, which parts to look at twice.
    Q_PROPERTY(QVariantList lines READ lines NOTIFY resultChanged)

public:
    explicit OcrEngine(const QString &tessdataPath, QObject *parent = nullptr);
    ~OcrEngine() override;

    bool isBusy() const { return m_busy; }
    QString text() const { return m_result.text(); }
    int wordCount() const { return m_result.count(); }
    qreal confidence() const { return m_result.meanConfidence(); }
    QSize imageSize() const { return m_result.imageSize(); }
    QString lastError() const { return m_lastError; }
    QVariantList fields() const;
    QVariantList lines() const;

    // languages is Tesseract's own spelling: "fra" or "fra+eng".
    // autoRotate tries the page at 90 and 270 degrees as well, and keeps
    // whichever reading scored best. See Settings::autoRotate.
    Q_INVOKABLE void recognise(const QUrl &imageUrl, const QString &languages,
                               bool autoRotate = true);

    // Tap-to-extract. Point is in the coordinates of the original image, scope is
    // a TextLayout::Scope. Returns { valid, text, x, y, width, height }, empty
    // when the tap hit nothing.
    //
    // A QVariantMap rather than a model: a selection is one transient answer to
    // one gesture, and wrapping it in a QAbstractListModel would be ceremony
    // around a single row.
    Q_INVOKABLE QVariantMap selectAt(int x, int y, int scope, int tolerance) const;

    // The scopes, so QML can name them instead of passing bare integers.
    Q_INVOKABLE int scopeWord() const { return TextLayout::Word; }
    Q_INVOKABLE int scopeLine() const { return TextLayout::Line; }
    Q_INVOKABLE int scopeParagraph() const { return TextLayout::Paragraph; }
    Q_INVOKABLE int scopeBlock() const { return TextLayout::Block; }
    Q_INVOKABLE int growScope(int scope) const;

    void clear();

signals:
    void busyChanged();
    void resultChanged();
    void lastErrorChanged();
    void finished();
    void failed(const QString &message);

private slots:
    void handleFinished();

private:
    struct Outcome
    {
        OcrResult result;
        QString error;
    };

    // Runs on the worker thread.
    Outcome run(const QString &path, const QString &languages,
                bool autoRotate);

    void setBusy(bool busy);
    void setLastError(const QString &error);

    // The directory holding the .traineddata files, and the datapaths to try
    // handing Tesseract - the meaning of that argument changed between its
    // versions. See the constructor.
    QString m_tessdataPath;
    QStringList m_datapathCandidates;
    OcrResult m_result;
    QString m_lastError;
    bool m_busy = false;

    QFutureWatcher<Outcome> m_watcher;

    // TessBaseAPI is not thread-safe and its init is slow - tens of milliseconds
    // per language, every time. One instance is kept and reinitialised only when
    // the language set changes; the mutex guards it because the worker thread the
    // pool hands out is not always the same one.
    tesseract::TessBaseAPI *m_api = nullptr;
    QString m_apiLanguages;
    QMutex m_apiMutex;
};

#endif // OCRENGINE_H
