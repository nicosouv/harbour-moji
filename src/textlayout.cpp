#include "textlayout.h"

#include <limits>

namespace {

// Distance from a point to the nearest edge of a rectangle, 0 inside it.
// Manhattan rather than euclidean: it is only ever compared against other
// distances and against a tolerance, and this avoids a square root per word on a
// page that can hold a few thousand of them.
int distanceTo(const QRect &box, const QPoint &point)
{
    int dx = 0;
    if (point.x() < box.left()) {
        dx = box.left() - point.x();
    } else if (point.x() > box.right()) {
        dx = point.x() - box.right();
    }

    int dy = 0;
    if (point.y() < box.top()) {
        dy = box.top() - point.y();
    } else if (point.y() > box.bottom()) {
        dy = point.y() - box.bottom();
    }

    return dx + dy;
}

} // namespace

namespace TextLayout {

int wordIndexAt(const OcrResult &result, const QPoint &point, int tolerance)
{
    const QVector<OcrWord> &words = result.words();

    // Containment first, and it wins outright: a point inside a word must never
    // be answered with a neighbour just because the neighbour's centre is closer.
    for (int i = 0; i < words.size(); ++i) {
        if (words.at(i).box.contains(point)) {
            return i;
        }
    }

    if (tolerance <= 0) {
        return -1;
    }

    int best = -1;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < words.size(); ++i) {
        const int distance = distanceTo(words.at(i).box, point);
        if (distance <= tolerance && distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }

    return best;
}

Selection selectWord(const OcrResult &result, int wordIndex, Scope scope)
{
    Selection selection;
    if (wordIndex < 0 || wordIndex >= result.count()) {
        return selection;
    }

    const OcrWord &word = result.words().at(wordIndex);
    selection.scope = scope;

    switch (scope) {
    case Word:
        selection.words = QVector<int>() << wordIndex;
        selection.box = word.box;
        break;
    case Line:
        selection.words = result.wordsInLine(word.line);
        selection.box = result.lineBox(word.line);
        break;
    case Paragraph:
        selection.words = result.wordsInParagraph(word.paragraph);
        selection.box = result.paragraphBox(word.paragraph);
        break;
    case Block:
        selection.words = result.wordsInBlock(word.block);
        selection.box = result.blockBox(word.block);
        break;
    }

    selection.text = textOf(result, selection.words);
    return selection;
}

Selection selectAt(const OcrResult &result, const QPoint &point, Scope scope,
                   int tolerance)
{
    return selectWord(result, wordIndexAt(result, point, tolerance), scope);
}

Selection selectInRect(const OcrResult &result, const QRect &rect)
{
    Selection selection;
    selection.scope = Word;

    const QVector<OcrWord> &words = result.words();
    for (int i = 0; i < words.size(); ++i) {
        if (rect.contains(words.at(i).box.center())) {
            selection.words.append(i);
            selection.box = selection.box.isNull()
                                ? words.at(i).box
                                : selection.box.united(words.at(i).box);
        }
    }

    selection.text = textOf(result, selection.words);
    return selection;
}

Scope grow(Scope scope)
{
    switch (scope) {
    case Word:      return Line;
    case Line:      return Paragraph;
    case Paragraph: return Block;
    case Block:     return Block;
    }
    return scope;
}

Scope shrink(Scope scope)
{
    switch (scope) {
    case Block:     return Paragraph;
    case Paragraph: return Line;
    case Line:      return Word;
    case Word:      return Word;
    }
    return scope;
}

QString textOf(const OcrResult &result, const QVector<int> &wordIndices)
{
    QString out;
    int previousLine = -1;
    int previousParagraph = -1;

    for (int index : wordIndices) {
        if (index < 0 || index >= result.count()) {
            continue;
        }
        const OcrWord &word = result.words().at(index);

        if (previousLine < 0) {
            // First word: no separator before it.
        } else if (word.paragraph != previousParagraph) {
            out += QLatin1String("\n\n");
        } else if (word.line != previousLine) {
            out += QLatin1Char('\n');
        } else {
            out += QLatin1Char(' ');
        }

        out += word.text;
        previousLine = word.line;
        previousParagraph = word.paragraph;
    }

    return out;
}

} // namespace TextLayout
