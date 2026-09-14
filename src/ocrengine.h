#ifndef OCRENGINE_H
#define OCRENGINE_H

#include <QFutureWatcher>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <QImage>
#include <QLineF>
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

    // What the user ended up with: the recognised text, unless they amended it.
    //
    // Correcting a single word writes back to the word it came from, so the boxes
    // and the checksums follow. Editing the whole block cannot do that - there is
    // no telling which word a new sentence belongs to - so this is kept beside the
    // result rather than inside it. Everything the user takes away (copy, share,
    // the history entry) comes from here; everything drawn on the photo still
    // comes from the words.
    Q_PROPERTY(QString editedText READ editedText WRITE setEditedText
                   NOTIFY editedTextChanged)
    Q_PROPERTY(bool edited READ isEdited NOTIFY editedTextChanged)
    Q_PROPERTY(int wordCount READ wordCount NOTIFY resultChanged)
    Q_PROPERTY(qreal confidence READ confidence NOTIFY resultChanged)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY resultChanged)

    // Which way up the photo had to be turned to be read, in degrees clockwise.
    //
    // Worth exposing because it is the only thing that knows. The Sailfish camera
    // writes an EXIF orientation of 1 on every frame it takes - the sensor's own
    // landscape frame, tagged as needing no rotation, however the phone was held -
    // so nothing upstream can say which way up the picture is, and the preview
    // came up sideways while the text came out fine. The recogniser found the
    // answer on the way past; the view can simply use it.
    Q_PROPERTY(int orientation READ orientation NOTIFY resultChanged)
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

    // The words the recogniser was unsure of, with their boxes, for highlighting
    // on the photo and offering for correction.
    //
    // Only the doubtful ones, never all of them: a page holds a couple of thousand
    // words and a Repeater over that stutters, while the ones worth showing are
    // usually a handful.
    Q_PROPERTY(QVariantList uncertainWords READ uncertainWords NOTIFY resultChanged)
    Q_PROPERTY(int uncertainCount READ uncertainCount NOTIFY resultChanged)

    // The blocks the recogniser found - a column, a panel, a caption. Offering
    // these is what lets a user take one piece of a busy page without cropping
    // the photo: the separation has already been worked out, it was simply never
    // shown.
    Q_PROPERTY(QVariantList blocks READ blocks NOTIFY resultChanged)

public:
    explicit OcrEngine(const QString &tessdataPath, QObject *parent = nullptr);
    ~OcrEngine() override;

    bool isBusy() const { return m_busy; }
    QString text() const { return m_result.text(); }
    int wordCount() const { return m_result.count(); }
    qreal confidence() const { return m_result.meanConfidence(); }
    QSize imageSize() const { return m_result.imageSize(); }
    int orientation() const { return m_result.orientation(); }
    QString lastError() const { return m_lastError; }
    QString editedText() const;
    void setEditedText(const QString &text);
    bool isEdited() const { return m_edited; }

    QVariantList fields() const;

    // The word boxes covering every sensitive field, in the photo's coordinates.
    QVector<QRect> sensitiveBoxes() const;
    QVariantList lines() const;
    QVariantList uncertainWords() const;
    int uncertainCount() const;
    QVariantList blocks() const;
    QVariantList tables() const;
    int sensitiveCount() const;

    // The text of one block, or of the whole page when block is negative.
    Q_INVOKABLE QString textOfBlock(int block) const;

    // The page with some blocks left out. Takes a list of block numbers.
    Q_INVOKABLE QString textExcluding(const QVariantList &blocks) const;

    // Writes the photo out as a PDF with the recognised text laid invisibly over
    // it: it looks exactly like the photograph and is fully searchable.
    Q_INVOKABLE bool exportPdf(const QUrl &imageUrl, const QString &path) const;

    // The blocks that read as tables, with how many columns each has. A block is
    // a table when its words pile into vertical bands separated by channels no
    // word crosses - nothing on the page has to declare itself one, and a ruled
    // table and a set of aligned columns are indistinguishable once you are only
    // looking at where the words are.
    Q_PROPERTY(QVariantList tables READ tables NOTIFY resultChanged)

    // The block as CSV, empty when it does not read as a table.
    Q_INVOKABLE QString csvOfBlock(int block) const;

    // How many sensitive numbers are on the page - IBANs, card numbers, passport
    // codes. Only the kinds that identify money or a person: an email address is
    // found too, and blacking it out by default would be deciding for the user
    // what they consider private.
    Q_PROPERTY(int sensitiveCount READ sensitiveCount NOTIFY resultChanged)

    // Writes a copy of the photo with those numbers painted over, flattened so
    // the covering is part of the image rather than something a viewer can
    // switch off. Returns false if there is nothing to hide or the file cannot
    // be written.
    Q_INVOKABLE bool exportRedacted(const QUrl &imageUrl, const QString &path) const;

    // Writes that CSV to a file. Returns false if there is no table or the file
    // cannot be written.
    Q_INVOKABLE bool exportCsv(const QString &path, int block) const;

    // Replaces a word the recogniser got wrong. Everything derived from the text
    // - the raw text, the selection, the checksummed fields - follows.
    Q_INVOKABLE void correctWord(int index, const QString &text);

    // languages is Tesseract's own spelling: "fra" or "fra+eng".
    // autoRotate tries the page at 90 and 270 degrees as well, and keeps
    // whichever reading scored best. See Settings::autoRotate.
    Q_INVOKABLE void recognise(const QUrl &imageUrl, const QString &languages,
                               bool autoRotate = true, bool enhance = true);

    // Recognises only part of the photo, in the photo's own coordinates.
    //
    // Tesseract separates a page into blocks well enough to pick one, but not
    // when it merges two columns into a single block - and then the only way to
    // say "this bit, not that bit" is to point at it. Boxes still come back in
    // the whole photo's coordinates, so the overlay needs no special case.
    Q_INVOKABLE void recogniseRegion(const QUrl &imageUrl, const QString &languages,
                                     bool autoRotate, bool enhance,
                                     int x, int y, int width, int height);

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
    void editedTextChanged();
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

    // One recognition pass over one image, filling boxes in *that image's*
    // coordinates. Both the quarter-turn search and the straightening use it, so
    // the iteration that assembles the hierarchy exists once.
    //
    // `pageSegMode` is a tesseract::PageSegMode and is always passed, never left
    // to the library: TessBaseAPI's default is PSM_SINGLE_BLOCK, which reads the
    // whole photograph as one uniform slab of text. This engine never set one, so
    // that is what shipped - and on a magazine page with an illustration beside
    // the column it turned 66 words at 92% into 124 words at 38%. The
    // command-line tool sets PSM_AUTO before every run; so does this now.
    //
    // Runs on the worker thread, with the API mutex already held.
    bool recogniseInto(const QImage &grey, int pageSegMode, OcrResult *out,
                       QVector<QLineF> *baselines);

    // The angles, in one place, because two loops search them.
    QVector<int> searchAngles(bool autoRotate) const;

    // The best reading of `prepared` at one segmentation mode, over every angle,
    // with boxes already mapped back into the source photo's coordinates.
    struct Attempt
    {
        OcrResult result;
        QVector<QLineF> baselines;
        int angle = 0;
        bool found = false;
    };

    Attempt bestOverAngles(const QImage &prepared, qreal scale, const QSize &sourceSize,
                           bool autoRotate, int pageSegMode);

    // Runs on the worker thread.
    Outcome run(const QString &path, const QString &languages,
                bool autoRotate, bool enhance, QRect region);

    void setBusy(bool busy);
    void setLastError(const QString &error);

    // The directory holding the .traineddata files, and the datapaths to try
    // handing Tesseract - the meaning of that argument changed between its
    // versions. See the constructor.
    QString m_tessdataPath;
    QStringList m_datapathCandidates;
    OcrResult m_result;
    QString m_lastError;
    QString m_editedText;
    bool m_edited = false;
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
