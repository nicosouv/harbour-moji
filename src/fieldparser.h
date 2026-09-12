#ifndef FIELDPARSER_H
#define FIELDPARSER_H

#include <QString>
#include <QVector>

// Finding the structured things inside recognised text, and checking them.
//
// The checking is the point. Plenty of apps can spot something IBAN-shaped with a
// regular expression; almost none will tell you whether the number is actually
// valid, which matters far more when the digits came out of a camera than when
// they were typed. OCR confuses 8 with B, 0 with O, 1 with l - exactly the errors
// a checksum exists to catch. So a field found here is either "valid" or "found
// but does not check out", and the second answer is more useful than a silent
// guess: it says look again at this line.
//
// Everything here is pure string work, deliberately - no Tesseract, no Qt Quick -
// so tests/ can hold it to known-good and known-bad values, which is the only way
// to be sure a checksum is right.
namespace FieldParser {

enum Kind {
    Unknown,
    Iban,
    CreditCard,
    Isbn,
    MrzLine,
    Email,
    Url
};

struct Field
{
    Kind kind = Unknown;

    // As it appeared in the text, so it can be highlighted in place.
    QString raw;

    // Cleaned up: spaces and separators removed, letters upper-cased. This is
    // what gets copied, and what the checksum was computed over.
    QString normalised;

    // Offset and length of raw within the text it was found in.
    int start = 0;
    int length = 0;

    // False when the shape matched but the check digits did not. Kinds that carry
    // no checksum (Email, Url) report true.
    bool checksumValid = false;
};

// Every field in the text, in the order they appear.
QVector<Field> scan(const QString &text);

// The individual checks, exposed because they are the part worth testing directly
// and the part a caller may want for a value the user typed or corrected by hand.
// Each takes a normalised string and tolerates spaces.

// ISO 13616: move the first four characters to the end, replace each letter with
// its position + 9, and the whole thing read as an integer must be 1 mod 97.
bool ibanValid(const QString &iban);

// Luhn, as used by every payment card and a good many membership numbers.
bool luhnValid(const QString &digits);

// ISBN-10 (mod 11, where the last character may be X for 10) or ISBN-13 (mod 10
// with alternating weights 1 and 3). Length decides which.
bool isbnValid(const QString &isbn);

// One check digit over a run of MRZ characters: weights cycle 7, 3, 1; digits
// count as themselves, A-Z as 10-35, and the filler '<' as 0.
int mrzCheckDigit(const QString &input);

// A TD3 machine-readable zone - the two 44-character lines at the bottom of a
// passport - with all four of its check digits verified.
bool mrzTd3Valid(const QString &line1, const QString &line2);

// Human-readable name for a kind. Not translated: the UI translates it, because a
// module that calls tr() decides the wording for every caller.
QString kindName(Kind kind);

} // namespace FieldParser

#endif // FIELDPARSER_H
