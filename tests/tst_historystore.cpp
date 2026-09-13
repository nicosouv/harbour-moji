#include <QtTest>

#include "historystore.h"

// What the app remembers having read.
//
// The rule worth pinning is that a photo has one entry, not one per reading: the
// usual sequence is to read a page, notice the language was wrong, change it and
// read again - and a history that showed that twice would be a log of attempts
// rather than a list of documents.
class TestHistoryStore : public QObject
{
    Q_OBJECT

private:
    static QString sqlite() { return QStringLiteral(":memory:"); }

private slots:
    void opensAndStartsEmpty();
    void remembersAReading();
    void newestComesFirst();
    void rereadingAPhotoReplacesItsEntry();
    void summaryIsTheFirstLine();
    void summaryFallsBackToTheFileName();
    void forgetRemovesOne();
    void forgetAllEmptiesIt();
    void textSurvivesWithoutThePhoto();
    void keepsOnlyTheMostRecent();
    void refusesToRememberWithoutAPath();
};

void TestHistoryStore::opensAndStartsEmpty()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));
    QVERIFY(store.isOpen());
    QCOMPARE(store.count(), 0);
    QVERIFY(store.entries().isEmpty());
}

void TestHistoryStore::remembersAReading()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    const int id = store.remember(QStringLiteral("/tmp/a.jpg"),
                                  QStringLiteral("fra"), 120, 88.5,
                                  QStringLiteral("Bonjour\nle monde"));
    QVERIFY(id > 0);
    QCOMPARE(store.count(), 1);

    const QVariantMap entry = store.entries().first().toMap();
    QCOMPARE(entry.value(QStringLiteral("imagePath")).toString(),
             QStringLiteral("/tmp/a.jpg"));
    QCOMPARE(entry.value(QStringLiteral("languages")).toString(), QStringLiteral("fra"));
    QCOMPARE(entry.value(QStringLiteral("wordCount")).toInt(), 120);
}

void TestHistoryStore::newestComesFirst()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    store.remember(QStringLiteral("/tmp/old.jpg"), QStringLiteral("eng"), 1, 50.0,
                   QStringLiteral("older"));
    QTest::qSleep(1100);   // read_at has one-second resolution
    store.remember(QStringLiteral("/tmp/new.jpg"), QStringLiteral("eng"), 1, 50.0,
                   QStringLiteral("newer"));

    QCOMPARE(store.entries().first().toMap().value(QStringLiteral("summary")).toString(),
             QStringLiteral("newer"));
}

void TestHistoryStore::rereadingAPhotoReplacesItsEntry()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    // Read once in the wrong language...
    store.remember(QStringLiteral("/tmp/page.jpg"), QStringLiteral("eng"), 40, 38.0,
                   QStringLiteral("garbled"));
    // ...then again in the right one.
    store.remember(QStringLiteral("/tmp/page.jpg"), QStringLiteral("fra"), 130, 89.0,
                   QStringLiteral("correct"));

    QCOMPARE(store.count(), 1);

    const QVariantMap entry = store.entries().first().toMap();
    QCOMPARE(entry.value(QStringLiteral("languages")).toString(), QStringLiteral("fra"));
    QCOMPARE(entry.value(QStringLiteral("wordCount")).toInt(), 130);
    QCOMPARE(entry.value(QStringLiteral("summary")).toString(), QStringLiteral("correct"));
}

void TestHistoryStore::summaryIsTheFirstLine()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));
    store.remember(QStringLiteral("/tmp/a.jpg"), QStringLiteral("fra"), 3, 90.0,
                   QStringLiteral("  Facture 2026  \nligne deux\nligne trois"));

    // Simplified, so the row is not indented by whatever the page was.
    QCOMPARE(store.entries().first().toMap().value(QStringLiteral("summary")).toString(),
             QStringLiteral("Facture 2026"));
}

void TestHistoryStore::summaryFallsBackToTheFileName()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    // A null QString, not an empty one: that is what a photo with no text in it
    // produces, and it binds as SQL NULL against a NOT NULL column.
    const int id = store.remember(QStringLiteral("/tmp/IMG_0042.jpg"),
                                  QStringLiteral("fra"), 0, 0.0, QString());
    QVERIFY2(id > 0, "a reading that found no text is still worth recording");
    QCOMPARE(store.count(), 1);

    QCOMPARE(store.entries().first().toMap().value(QStringLiteral("summary")).toString(),
             QStringLiteral("IMG_0042.jpg"));
}

void TestHistoryStore::forgetRemovesOne()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    const int id = store.remember(QStringLiteral("/tmp/a.jpg"), QStringLiteral("eng"),
                                  1, 50.0, QStringLiteral("a"));
    store.remember(QStringLiteral("/tmp/b.jpg"), QStringLiteral("eng"), 1, 50.0,
                   QStringLiteral("b"));

    store.forget(id);

    QCOMPARE(store.count(), 1);
    QCOMPARE(store.entries().first().toMap().value(QStringLiteral("summary")).toString(),
             QStringLiteral("b"));
}

void TestHistoryStore::forgetAllEmptiesIt()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));
    store.remember(QStringLiteral("/tmp/a.jpg"), QStringLiteral("eng"), 1, 50.0,
                   QStringLiteral("a"));

    store.forgetAll();

    QCOMPARE(store.count(), 0);
}

void TestHistoryStore::textSurvivesWithoutThePhoto()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    // A path that does not exist: the photo was deleted from the gallery after
    // being read. The text has to remain readable, and the entry has to say the
    // photo is gone so the UI does not offer to read it again.
    const int id = store.remember(QStringLiteral("/tmp/definitely-not-here.jpg"),
                                  QStringLiteral("fra"), 5, 80.0,
                                  QStringLiteral("kept anyway"));

    QCOMPARE(store.textOf(id), QStringLiteral("kept anyway"));
    QCOMPARE(store.entries().first().toMap().value(QStringLiteral("imageExists")).toBool(),
             false);
}

void TestHistoryStore::keepsOnlyTheMostRecent()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));

    for (int i = 0; i < HistoryStore::MaxEntries + 10; ++i) {
        store.remember(QStringLiteral("/tmp/%1.jpg").arg(i), QStringLiteral("eng"),
                       1, 50.0, QStringLiteral("page %1").arg(i));
    }

    QCOMPARE(store.count(), HistoryStore::MaxEntries);
}

void TestHistoryStore::refusesToRememberWithoutAPath()
{
    HistoryStore store;
    QVERIFY(store.open(sqlite()));
    QCOMPARE(store.remember(QString(), QStringLiteral("eng"), 1, 50.0,
                            QStringLiteral("x")), -1);
    QCOMPARE(store.count(), 0);
}

QTEST_MAIN(TestHistoryStore)
#include "tst_historystore.moc"
