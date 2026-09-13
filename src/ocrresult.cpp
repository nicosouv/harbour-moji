#include "ocrresult.h"

void OcrResult::clear()
{
    m_words.clear();
    m_imageSize = QSize();
    m_orientation = 0;
}

QString OcrResult::text() const
{
    QString out;
    int previousLine = -1;
    int previousParagraph = -1;

    for (const OcrWord &word : m_words) {
        if (previousLine < 0) {
            // First word: no separator before it.
        } else if (word.paragraph != previousParagraph) {
            // A blank line between paragraphs, so the raw text keeps the
            // structure the recogniser found instead of flattening to one block.
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

QRect OcrResult::boxOf(int OcrWord::*member, int value) const
{
    QRect box;
    for (const OcrWord &word : m_words) {
        if (word.*member == value) {
            // A null QRect unites badly - united() with a null rect returns the
            // other one, but only because QRect::isNull is checked first, so the
            // first assignment has to be a plain copy.
            box = box.isNull() ? word.box : box.united(word.box);
        }
    }
    return box;
}

QVector<int> OcrResult::indicesOf(int OcrWord::*member, int value) const
{
    QVector<int> indices;
    for (int i = 0; i < m_words.size(); ++i) {
        if (m_words.at(i).*member == value) {
            indices.append(i);
        }
    }
    return indices;
}

QRect OcrResult::lineBox(int line) const { return boxOf(&OcrWord::line, line); }
QRect OcrResult::paragraphBox(int paragraph) const { return boxOf(&OcrWord::paragraph, paragraph); }
QRect OcrResult::blockBox(int block) const { return boxOf(&OcrWord::block, block); }

QVector<int> OcrResult::wordsInLine(int line) const { return indicesOf(&OcrWord::line, line); }
QVector<int> OcrResult::wordsInParagraph(int paragraph) const { return indicesOf(&OcrWord::paragraph, paragraph); }
QVector<int> OcrResult::wordsInBlock(int block) const { return indicesOf(&OcrWord::block, block); }

void OcrResult::setWordText(int index, const QString &text)
{
    if (index < 0 || index >= m_words.size()) {
        return;
    }

    OcrWord &word = m_words[index];
    word.text = text;
    word.corrected = true;

    // A retyped word is as certain as anything on the page gets. Leaving the
    // recogniser's score would keep it highlighted as doubtful after the user has
    // just told us what it says.
    word.confidence = 100.0f;
}

QVector<int> OcrResult::uncertainWords(float threshold) const
{
    QVector<int> indices;
    for (int i = 0; i < m_words.size(); ++i) {
        const OcrWord &word = m_words.at(i);
        if (!word.corrected && word.confidence < threshold) {
            indices.append(i);
        }
    }
    return indices;
}

QVector<int> OcrResult::lineNumbers() const
{
    QVector<int> numbers;
    for (const OcrWord &word : m_words) {
        // Reading order, and words arrive in it, so the last one seen is the only
        // one worth comparing against - no set, no sort.
        if (numbers.isEmpty() || numbers.last() != word.line) {
            if (!numbers.contains(word.line)) {
                numbers.append(word.line);
            }
        }
    }
    return numbers;
}

QVector<int> OcrResult::blockNumbers() const
{
    QVector<int> numbers;
    for (const OcrWord &word : m_words) {
        if (!numbers.contains(word.block)) {
            numbers.append(word.block);
        }
    }
    return numbers;
}

QString OcrResult::blockText(int block) const
{
    QString out;
    int previousLine = -1;
    int previousParagraph = -1;

    for (const OcrWord &word : m_words) {
        if (word.block != block) {
            continue;
        }
        if (previousLine < 0) {
            // First word of the block.
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

float OcrResult::lineConfidence(int line) const
{
    float total = 0.0f;
    int count = 0;
    for (const OcrWord &word : m_words) {
        if (word.line == line) {
            total += word.confidence;
            ++count;
        }
    }
    return count > 0 ? total / count : 0.0f;
}

float OcrResult::meanConfidence() const
{
    if (m_words.isEmpty()) {
        return 0.0f;
    }

    float total = 0.0f;
    for (const OcrWord &word : m_words) {
        total += word.confidence;
    }
    return total / m_words.size();
}

float OcrResult::readingScore() const
{
    return m_words.size() * meanConfidence();
}
