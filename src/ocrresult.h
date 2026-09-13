#ifndef OCRRESULT_H
#define OCRRESULT_H

#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>

// What one recognition pass produced, in the shape Tesseract's ResultIterator
// hands it over: a flat list of words, each carrying the index of the line,
// paragraph and block it belongs to.
//
// Flat rather than a tree on purpose. Every interesting question - which word was
// tapped, what is the box around its paragraph, which words fall inside a
// dragged rectangle - is a scan with a filter, and a tree would have to be walked
// for all of them while making the Tesseract translation longer. The hierarchy is
// still fully present; it is just carried as indices.
//
// No Tesseract types appear here, which is what lets tests/ link this without the
// cross-compiled library.
struct OcrWord
{
    QString text;
    QRect box;

    // 0..100, as Tesseract reports it. Below ~60 a word is usually wrong rather
    // than slightly wrong, which is worth showing the user rather than hiding.
    float confidence = 0.0f;

    int line = 0;
    int paragraph = 0;
    int block = 0;
};

class OcrResult
{
public:
    OcrResult() = default;

    // Size of the image the boxes are relative to. The UI scales an overlay by
    // the ratio between this and the displayed size, so a result is useless
    // without it.
    QSize imageSize() const { return m_imageSize; }
    void setImageSize(const QSize &size) { m_imageSize = size; }

    // Degrees clockwise that had to be undone to read the page, from osd. Kept so
    // the overlay can be rotated back onto the original photo.
    int orientation() const { return m_orientation; }
    void setOrientation(int degrees) { m_orientation = degrees; }

    const QVector<OcrWord> &words() const { return m_words; }
    void append(const OcrWord &word) { m_words.append(word); }
    void clear();

    bool isEmpty() const { return m_words.isEmpty(); }
    int count() const { return m_words.size(); }

    // Every word, with a space between words and a newline between lines. This is
    // the "raw text" the user copies or exports.
    QString text() const;

    // The smallest rectangle containing every word of the given group.
    QRect lineBox(int line) const;
    QRect paragraphBox(int paragraph) const;
    QRect blockBox(int block) const;

    // The distinct line numbers, in reading order. Used to draw where the text
    // is without instantiating one item per word: a page can hold a couple of
    // thousand words and only a few dozen lines, and on a phone that difference
    // is the difference between a smooth overlay and a stuttering one.
    QVector<int> lineNumbers() const;

    // Mean confidence over one line, for the same reason: a page's weak spots are
    // legible per line and meaningless per word.
    float lineConfidence(int line) const;

    // Indices of the words in a group, in reading order.
    QVector<int> wordsInLine(int line) const;
    QVector<int> wordsInParagraph(int paragraph) const;
    QVector<int> wordsInBlock(int block) const;

    // Mean confidence, weighted by nothing: a long word and a short one are
    // equally likely to be the one that is wrong.
    float meanConfidence() const;

private:
    QRect boxOf(int OcrWord::*member, int value) const;
    QVector<int> indicesOf(int OcrWord::*member, int value) const;

    QVector<OcrWord> m_words;
    QSize m_imageSize;
    int m_orientation = 0;
};

#endif // OCRRESULT_H
