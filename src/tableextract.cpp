#include "tableextract.h"

#include <QRect>
#include <QStringList>

#include <algorithm>

namespace {

// The median height of the words in a block, used as the scale everything else
// is measured against. Median rather than mean: one word box blown up by a smear
// on the page should not redefine what "a wide gap" means.
qreal medianWordHeight(const OcrResult &result, int block)
{
    QVector<int> heights;
    for (const OcrWord &word : result.words()) {
        if (word.block == block) {
            heights.append(word.box.height());
        }
    }
    if (heights.isEmpty()) {
        return 0.0;
    }
    std::sort(heights.begin(), heights.end());
    return heights.at(heights.size() / 2);
}

} // namespace

namespace TableExtract {

QVector<int> columnEdges(const OcrResult &result, int block, qreal minGap)
{
    const qreal scale = medianWordHeight(result, block);
    if (scale <= 0.0) {
        return QVector<int>();
    }

    // Every horizontal span occupied by a word, merged into a set of bands. What
    // is left between the bands is vertical whitespace running the height of the
    // block - and a channel wide enough is a column boundary.
    QVector<QPair<int, int>> spans;
    for (const OcrWord &word : result.words()) {
        if (word.block == block) {
            spans.append(qMakePair(word.box.left(), word.box.right()));
        }
    }
    if (spans.size() < 4) {
        // Too little to tell a table from a sentence.
        return QVector<int>();
    }

    std::sort(spans.begin(), spans.end());

    QVector<QPair<int, int>> merged;
    merged.append(spans.first());
    for (int i = 1; i < spans.size(); ++i) {
        if (spans.at(i).first <= merged.last().second) {
            merged.last().second = qMax(merged.last().second, spans.at(i).second);
        } else {
            merged.append(spans.at(i));
        }
    }

    const qreal threshold = scale * minGap;

    QVector<int> edges;
    for (int i = 1; i < merged.size(); ++i) {
        const int gap = merged.at(i).first - merged.at(i - 1).second;
        if (gap >= threshold) {
            // The boundary sits in the middle of the channel, so a word that
            // overhangs slightly still falls on the right side of it.
            edges.append((merged.at(i - 1).second + merged.at(i).first) / 2);
        }
    }

    return edges;
}

int columnCount(const OcrResult &result, int block, qreal minGap)
{
    const QVector<int> edges = columnEdges(result, block, minGap);
    return edges.isEmpty() ? 0 : edges.size() + 1;
}

QVector<QVector<QString>> cells(const OcrResult &result, int block, qreal minGap)
{
    QVector<QVector<QString>> rows;

    const QVector<int> edges = columnEdges(result, block, minGap);
    if (edges.isEmpty()) {
        return rows;
    }
    const int columns = edges.size() + 1;

    // One row per text line of the block, in reading order.
    for (int line : result.lineNumbers()) {
        QVector<QString> row(columns);
        bool any = false;

        for (int index : result.wordsInLine(line)) {
            const OcrWord &word = result.words().at(index);
            if (word.block != block) {
                continue;
            }

            // Placed by its centre, not its left edge: a word that starts just
            // before a boundary and ends well past it belongs to the column it
            // sits in, not the one it touches.
            const int centre = word.box.center().x();
            int column = 0;
            while (column < edges.size() && centre > edges.at(column)) {
                ++column;
            }

            if (!row[column].isEmpty()) {
                row[column] += QLatin1Char(' ');
            }
            row[column] += word.text;
            any = true;
        }

        if (any) {
            rows.append(row);
        }
    }

    return rows;
}

QString escapeField(const QString &field)
{
    const bool needsQuotes = field.contains(QLatin1Char(','))
                             || field.contains(QLatin1Char('"'))
                             || field.contains(QLatin1Char('\n'))
                             || field.contains(QLatin1Char('\r'));
    if (!needsQuotes) {
        return field;
    }

    QString escaped = field;
    escaped.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString toCsv(const OcrResult &result, int block, qreal minGap)
{
    const QVector<QVector<QString>> rows = cells(result, block, minGap);
    if (rows.isEmpty()) {
        return QString();
    }

    QStringList lines;
    for (const QVector<QString> &row : rows) {
        QStringList fields;
        for (const QString &cell : row) {
            fields.append(escapeField(cell));
        }
        lines.append(fields.join(QLatin1Char(',')));
    }

    return lines.join(QLatin1Char('\n'));
}

} // namespace TableExtract
