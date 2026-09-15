#include "logging.h"

// Warnings and worse by default; debug only when asked for.
//
// The two-argument form is the whole of this file's reason to exist. Without it a
// category is enabled at every level, so every qCDebug in the app went to the
// journal on a shipped device - and this app's debug lines carry the paths of the
// documents being read. A filename is often the most private part of a document,
// and the journal is not the place for it.
//
// The comment in the header always claimed you turned logging on with
// QT_LOGGING_RULES, which implied it was off. It was not. Now it is, and that
// spelling really does turn it on.
Q_LOGGING_CATEGORY(lcMoji, "harbour.moji", QtWarningMsg)
