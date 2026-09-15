#ifndef LOGGING_H
#define LOGGING_H

#include <QLoggingCategory>

// One category for the whole app, so a user can turn it on without drowning:
//   QT_LOGGING_RULES="harbour.moji.debug=true" harbour-moji
//
// And off by default - see logging.cpp, which is the whole point of that line.
Q_DECLARE_LOGGING_CATEGORY(lcMoji)

#endif // LOGGING_H
