#ifndef LANGUAGENAMES_H
#define LANGUAGENAMES_H

#include <QLocale>
#include <QString>
#include <QStringList>

// What a Tesseract language code is called, for the picker.
//
// This was tried through QLocale first, and QLocale cannot do it. Qt 5's
// QLocale("fra") resolves nothing: its table holds ISO 639-1 ("fr"), and the
// three-letter entries it does carry are only for languages that have no
// two-letter code. So every one of Tesseract's ISO 639-2/T codes fell through to
// the honest fallback and the picker listed "fra", "ces", "ell", "chi_sim" - the
// codes themselves, which is what a user actually saw.
//
// Hence a table. It is 126 lines plus the two non-languages Tesseract ships, and
// it is the whole set from tessdata-manifest.txt rather than the thirty in the
// base package, because a language pack installed later has to appear named.
//
// Deliberately not translated. Two names per language in two catalogues is 500
// entries for a UI that is English and French, and CLAUDE.md is explicit that the
// interface list and the recognition list must not grow into each other. The
// endonym needs no translation by definition, and the English gloss is the one
// that can be read by everybody this app is translated for.
namespace LanguageNames {

// The language's own name for itself: "Français", "Ελληνικά", "日本語".
// Empty for a code not in the table.
QString endonym(const QString &code);

// Its name in English: "French", "Greek", "Japanese". Empty for an unknown code.
QString englishName(const QString &code);

// What the picker shows: the endonym, plus the English name in brackets when the
// two differ.
//
// The gloss is there because the choice is about a document, not about the
// reader. Somebody photographing a Finnish page is not Finnish - they are looking
// for "Finnish" and would never find "Suomi" - while "English" or "Esperanto"
// gains nothing from being said twice.
//
// An unknown code comes back as itself. A wrong name would be worse than a raw
// one, and a language pack from a Tesseract newer than this table is exactly the
// case that produces one.
QString displayName(const QString &code);

// Whether the code names something that can be read in. Tesseract ships two files
// that cannot: osd detects orientation and script, equ detects mathematics.
// Offering either as a language offers to recognise text with a model that
// recognises none.
bool isReadable(const QString &code);

// Which of `available` a phone set to `locale` should read in by default, or an
// empty string when none of them fits.
//
// Here for the same reason as the names, and it is the same bug: the default used
// to be found by comparing QLocale(code).language() against the system's, which
// for "fra" compares QLocale::C against QLocale::French and never matches. So a
// French phone silently defaulted to English and every accented word came back
// slightly wrong - the setting was right in Settings, it was just never the one
// that had been reached for.
//
// Matched on the ISO 639-1 code the table carries beside each language, because
// that is the half of the pair QLocale speaks.
QString codeForLocale(const QLocale &locale, const QStringList &available);

} // namespace LanguageNames

#endif // LANGUAGENAMES_H
