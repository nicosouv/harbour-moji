#ifndef TABLEEXTRACT_H
#define TABLEEXTRACT_H

#include <QString>
#include <QVector>

#include "ocrresult.h"

// Reading a table out of a page, without the page ever having said it is one.
//
// Tesseract reports words, lines and blocks; it does not report columns. But a
// table is exactly a block whose words pile up into a few vertical bands, and
// that is measurable: gather the horizontal extent of every word in the block,
// find the gaps that no word crosses, and those gaps are the column boundaries.
//
// Nothing here needs the table to be ruled. A ruled table and a set of aligned
// columns look identical once you are only looking at where the words are, which
// is why this works on a receipt as well as on a spreadsheet printout.
//
// Pure, so tests/ can hold it to grids whose right answer is known.
namespace TableExtract {

// The x positions where one column ends and the next begins, in the coordinates
// of the result's image. Empty when the block does not read as a table.
//
// minGap is how wide a vertical channel has to be before it counts as a column
// boundary rather than a word space, as a multiple of the median word height -
// measuring it against the text's own size is what lets one number work for a
// photograph taken from any distance.
QVector<int> columnEdges(const OcrResult &result, int block, qreal minGap = 0.9);

// Rows of cells. Each row is one text line of the block, each cell the words that
// fell in that column, joined by spaces. Short rows are padded so every row has
// the same number of cells, because a CSV with ragged rows is not a CSV.
QVector<QVector<QString>> cells(const OcrResult &result, int block,
                                qreal minGap = 0.9);

// RFC 4180: fields containing a comma, a quote or a newline are quoted, and a
// quote inside a field is doubled. Worth doing properly - a receipt total of
// "1,50" silently splitting a column is the kind of wrong that is only noticed
// much later, in a spreadsheet.
QString escapeField(const QString &field);

// The block as CSV, or an empty string when it does not read as a table.
QString toCsv(const OcrResult &result, int block, qreal minGap = 0.9);

// How many columns a block has, for deciding whether to offer the export at all.
// One column is a paragraph, not a table.
int columnCount(const OcrResult &result, int block, qreal minGap = 0.9);

} // namespace TableExtract

#endif // TABLEEXTRACT_H
