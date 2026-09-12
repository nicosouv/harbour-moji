#ifndef TEXTLAYOUT_H
#define TEXTLAYOUT_H

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include "ocrresult.h"

// Turning a place on the image into a piece of text.
//
// This is the layer the tap-to-extract gesture is made of, and it is deliberately
// pure: no Tesseract, no QImage, no models. Given a result and a point it answers
// which word, and given a scope it answers which line, paragraph or block that
// word belongs to. Every decision that makes the gesture feel right or wrong -
// how far off a tap may be, whether a half-covered word counts as selected - is
// made here, where a test can pin it.
namespace TextLayout {

// Growing outward from a tap. This is the whole idea: a tap lands on a word, and
// tapping again widens the selection to the structure the recogniser actually
// found rather than to a rectangle the user has to drag.
enum Scope {
    Word,
    Line,
    Paragraph,
    Block
};

struct Selection
{
    Scope scope = Word;
    QVector<int> words;
    QRect box;
    QString text;

    bool isValid() const { return !words.isEmpty(); }
};

// Index of the word under the point, or -1.
//
// A tap that lands between two words hits nothing at all, because Tesseract's
// boxes are tight to the glyphs and a fingertip is not. So a miss falls back to
// the nearest word within tolerance, measured from the edge of its box. Passing
// tolerance 0 gives the strict containment test, which is what the tests use to
// check the containment path on its own.
int wordIndexAt(const OcrResult &result, const QPoint &point, int tolerance = 0);

// The selection containing the given point at the given scope. Invalid if the
// point hit nothing.
Selection selectAt(const OcrResult &result, const QPoint &point, Scope scope,
                   int tolerance = 0);

// The selection for a word index already known.
Selection selectWord(const OcrResult &result, int wordIndex, Scope scope);

// Every word whose centre falls inside the rectangle - a dragged selection.
//
// Centre, not intersection: a rectangle dragged past the edge of a column clips
// the first letter of the next one, and intersection would then pull in a word the
// user plainly did not include.
Selection selectInRect(const OcrResult &result, const QRect &rect);

// One step out, one step in. Both saturate rather than wrapping, so repeated taps
// settle at the block instead of cycling back to the word.
Scope grow(Scope scope);
Scope shrink(Scope scope);

// The words joined the way the raw text joins them - space inside a line, newline
// between lines, blank line between paragraphs.
QString textOf(const OcrResult &result, const QVector<int> &wordIndices);

} // namespace TextLayout

#endif // TEXTLAYOUT_H
