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

    // Set when the user retyped this word. It stops being offered for correction
    // and stops counting against the page's confidence: the recogniser's doubt
    // was about its own reading, and that reading has been replaced.
    bool corrected = false;
};

// Below this a word is usually wrong rather than slightly wrong, which is the
// point at which showing it to the user is worth the interruption. Tesseract's
// scale is 0-100.
const float LowConfidence = 70.0f;

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

    // Replaces what a word says, because the user retyped it.
    //
    // The correction has to flow everywhere the original did - the raw text, any
    // selection containing it, and the field scan - which is why it is done here
    // rather than kept as an overlay somewhere in the UI. Correcting a digit in a
    // misread IBAN and watching it turn from "does not match" to "verified" is
    // the whole reason this is worth having.
    void setWordText(int index, const QString &text);

    // Indices of the words worth offering for correction: low confidence, and not
    // already corrected.
    QVector<int> uncertainWords(float threshold = LowConfidence) const;

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

    // The distinct block numbers, in reading order.
    //
    // A block is what the recogniser decided is one piece of writing - a column, a
    // panel, a caption. Photograph a leaflet and the column next door comes along
    // as its own block, which is exactly what makes "just this one" possible
    // without cropping the photo or recognising it twice.
    QVector<int> blockNumbers() const;

    // The text of one block, joined the way text() joins the whole page.
    QString blockText(int block) const;

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

    // How well this reading went, as one number, for choosing between the same
    // page recognised at different rotations.
    //
    // Mean confidence times the square root of the word count. The square root is
    // the whole point, and it was learned the hard way from a real page:
    //
    //     angle   0 : 321 words, confidence 37.6
    //     angle  90 : 135 words, confidence 74.7   <- plainly the right way up
    //     angle 270 : 151 words, confidence 36.6
    //
    // A plain product picks 0 degrees, because a page read sideways does not find
    // *less* - it finds far more, all of it fragments the recogniser openly
    // doubts. Confidence is the recogniser's own estimate of whether it read
    // correctly, so it must dominate; word count only has to stop a pass that
    // found almost nothing from winning on certainty alone. Damping it to a square
    // root does both: 74.7*sqrt(135) beats 37.6*sqrt(321), while 80*sqrt(300)
    // still beats 95*sqrt(8).
    float readingScore() const;

private:
    QRect boxOf(int OcrWord::*member, int value) const;
    QVector<int> indicesOf(int OcrWord::*member, int value) const;

    QVector<OcrWord> m_words;
    QSize m_imageSize;
    int m_orientation = 0;
};

#endif // OCRRESULT_H
