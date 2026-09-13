#include "historystore.h"

#include <QDateTime>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include "logging.h"

HistoryStore::HistoryStore(QObject *parent)
    : QObject(parent)
    // A connection name of its own, so a test can open several stores at once
    // and so this never collides with any other part of the app.
    , m_connectionName(QStringLiteral("moji-history-")
                       + QUuid::createUuid().toString())
{
}

HistoryStore::~HistoryStore()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
    m_database = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool HistoryStore::open(const QString &path)
{
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(path);

    if (!m_database.open()) {
        qCWarning(lcMoji) << "cannot open the history at" << path
                          << m_database.lastError().text();
        return false;
    }

    QSqlQuery query(m_database);
    const bool created = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS readings ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  image_path TEXT NOT NULL,"
        "  languages TEXT NOT NULL,"
        "  word_count INTEGER NOT NULL,"
        "  confidence REAL NOT NULL,"
        "  text TEXT NOT NULL,"
        "  read_at INTEGER NOT NULL"
        ")"));

    if (!created) {
        qCWarning(lcMoji) << "cannot create the history table"
                          << query.lastError().text();
        return false;
    }

    // One entry per photo, enforced here rather than by remembering to check:
    // re-reading a page in another language must replace its row.
    query.exec(QStringLiteral(
        "CREATE UNIQUE INDEX IF NOT EXISTS readings_path ON readings (image_path)"));

    emit changed();
    return true;
}

bool HistoryStore::isOpen() const
{
    return m_database.isOpen();
}

int HistoryStore::remember(const QString &imagePath, const QString &languages,
                           int wordCount, qreal confidence, const QString &text)
{
    if (!isOpen() || imagePath.isEmpty()) {
        return -1;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO readings (image_path, languages, word_count, confidence, text, read_at)"
        " VALUES (?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(image_path) DO UPDATE SET"
        "   languages = excluded.languages,"
        "   word_count = excluded.word_count,"
        "   confidence = excluded.confidence,"
        "   text = excluded.text,"
        "   read_at = excluded.read_at"));
    query.addBindValue(imagePath);
    query.addBindValue(languages);
    query.addBindValue(wordCount);
    query.addBindValue(confidence);
    // A null QString binds as SQL NULL, and the column is NOT NULL - so a photo
    // that turned out to have no text in it would fail to be recorded at all,
    // quietly. An empty reading is still a reading worth remembering.
    query.addBindValue(text.isNull() ? QString(QLatin1String("")) : text);
    // Not currentSecsSinceEpoch(): that arrived in Qt 5.8 and Sailfish ships 5.6.
    // The milliseconds form has been there since 4.7.
    query.addBindValue(QDateTime::currentMSecsSinceEpoch() / 1000);

    if (!query.exec()) {
        qCWarning(lcMoji) << "cannot record a reading" << query.lastError().text();
        return -1;
    }

    trim();
    emit changed();

    QSqlQuery lookup(m_database);
    lookup.prepare(QStringLiteral("SELECT id FROM readings WHERE image_path = ?"));
    lookup.addBindValue(imagePath);
    if (lookup.exec() && lookup.next()) {
        return lookup.value(0).toInt();
    }
    return -1;
}

void HistoryStore::trim()
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "DELETE FROM readings WHERE id NOT IN ("
        "  SELECT id FROM readings ORDER BY read_at DESC LIMIT ?)"));
    query.addBindValue(MaxEntries);
    query.exec();
}

QVariantList HistoryStore::entries() const
{
    QVariantList list;
    if (!isOpen()) {
        return list;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT id, image_path, languages, word_count, confidence, text, read_at"
            " FROM readings ORDER BY read_at DESC"))) {
        return list;
    }

    while (query.next()) {
        const QString path = query.value(1).toString();
        const QString text = query.value(5).toString();

        QVariantMap map;
        map.insert(QStringLiteral("id"), query.value(0).toInt());
        map.insert(QStringLiteral("imagePath"), path);
        map.insert(QStringLiteral("languages"), query.value(2).toString());
        map.insert(QStringLiteral("wordCount"), query.value(3).toInt());
        map.insert(QStringLiteral("confidence"), query.value(4).toDouble());
        map.insert(QStringLiteral("readAt"), query.value(6).toLongLong());

        // The first line, for the row's title: a page is recognised by what it
        // says far faster than by the file name the camera gave it.
        const QString firstLine = text.section(QLatin1Char('\n'), 0, 0).simplified();
        map.insert(QStringLiteral("summary"),
                   firstLine.isEmpty() ? QFileInfo(path).fileName() : firstLine);

        // Whether it can be read again, or only re-read from what was stored.
        map.insert(QStringLiteral("imageExists"), QFileInfo::exists(path));

        list.append(map);
    }

    return list;
}

int HistoryStore::count() const
{
    if (!isOpen()) {
        return 0;
    }
    QSqlQuery query(m_database);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM readings")) && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QString HistoryStore::textOf(int id) const
{
    if (!isOpen()) {
        return QString();
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT text FROM readings WHERE id = ?"));
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

void HistoryStore::forget(int id)
{
    if (!isOpen()) {
        return;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM readings WHERE id = ?"));
    query.addBindValue(id);
    query.exec();
    emit changed();
}

void HistoryStore::forgetAll()
{
    if (!isOpen()) {
        return;
    }
    QSqlQuery query(m_database);
    query.exec(QStringLiteral("DELETE FROM readings"));
    emit changed();
}
