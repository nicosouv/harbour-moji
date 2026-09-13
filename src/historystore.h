#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

// What has been read before.
//
// Stores the text, not the photo: the photo is already on the device and keeping
// a second copy of every page anyone has ever pointed the camera at would grow
// without bound and duplicate the gallery. The path is remembered so the original
// can be reopened, and the entry survives the file being deleted - it just cannot
// be re-read then, which the UI has to allow for.
//
// Qt5Sql only, no Tesseract and no Qt Quick, so tests/ can reach it.
class HistoryStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    explicit HistoryStore(QObject *parent = nullptr);
    ~HistoryStore() override;

    // ":memory:" is accepted, which is what the tests use.
    bool open(const QString &path);
    bool isOpen() const;

    // Newest first.
    QVariantList entries() const;
    int count() const;

    // Returns the row id, or -1.
    //
    // Re-reading the same photo replaces its entry rather than adding a second:
    // correcting the language and reading again is one act, and a history that
    // showed it twice would be a log rather than a list of documents.
    Q_INVOKABLE int remember(const QString &imagePath, const QString &languages,
                             int wordCount, qreal confidence, const QString &text);

    Q_INVOKABLE void forget(int id);
    Q_INVOKABLE void forgetAll();

    // The stored text of one entry, for showing it without reading the photo
    // again - which matters when the photo is gone.
    Q_INVOKABLE QString textOf(int id) const;

    // How many entries are kept. Older ones are dropped on insert.
    static const int MaxEntries = 50;

signals:
    void changed();

private:
    void trim();

    QSqlDatabase m_database;
    QString m_connectionName;
};

#endif // HISTORYSTORE_H
