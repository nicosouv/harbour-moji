#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickView>
#include <QTranslator>

#include "logging.h"
#include "settings.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(new QGuiApplication(argc, argv));
    app->setApplicationName(QStringLiteral("harbour-moji"));
    app->setOrganizationName(QStringLiteral("harbour-moji"));

    const QString appDir = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/../share/harbour-moji");

    const QString tessdataPath = appDir + QStringLiteral("/tessdata");
    Settings *settings = new Settings(tessdataPath, app.data());

    // Language: the system locale. Unlike the OCR languages, this is the
    // interface, and Sailfish users expect it to follow the system.
    QTranslator *translator = new QTranslator(app.data());
    const QString locale = QLocale::system().name();
    const QString i18nDir = appDir + QStringLiteral("/translations");
    if (translator->load(QStringLiteral("harbour-moji-") + locale, i18nDir)
        || translator->load(QStringLiteral("harbour-moji-")
                                + locale.section(QLatin1Char('_'), 0, 0),
                            i18nDir)) {
        app->installTranslator(translator);
        qCDebug(lcMoji) << "loaded translation for" << locale;
    }

    QScopedPointer<QQuickView> view(new QQuickView);

    // qml/ on the import path so "import Mochi 1.0" resolves the same way from
    // every depth. Without it only qml/harbour-moji.qml could find the module,
    // because the implicit path is the importing file's own directory, and pages/
    // would have to say "../Mochi" instead. That second spelling is not merely
    // uglier: a directory import and a module import are distinct identities to
    // the engine, so Tokens - a singleton holding the theme - would exist twice,
    // and setting the theme through one would leave the components reading the
    // other.
    view->engine()->addImportPath(appDir + QStringLiteral("/qml"));

    view->rootContext()->setContextProperty(QStringLiteral("settings"), settings);
    view->rootContext()->setContextProperty(QStringLiteral("tessdataPath"), tessdataPath);
#ifdef APP_VERSION
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                            QStringLiteral(APP_VERSION));
#else
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                            QStringLiteral("dev"));
#endif

    view->setSource(QUrl::fromLocalFile(appDir + QStringLiteral("/qml/harbour-moji.qml")));
    view->showFullScreen();

    return app->exec();
}
