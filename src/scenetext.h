#ifndef SCENETEXT_H
#define SCENETEXT_H

#include <QRect>
#include <QSize>
#include <QVector>

// Turning a text detector's probability map into boxes.
//
// This is the first piece of the scene-text engine, and it is deliberately the
// piece with no neural network in it.
//
// Why a second engine at all. Tesseract is a document recogniser: it is trained
// on clean scans near 300 DPI and it decides where the text is by looking for a
// *page* - columns, paragraphs, reading order. A photograph of a street sign is
// not a page, and asked for one Tesseract returns nothing at all. Measured here,
// on a night photograph of "PASSAGE SURELEVE": zero words at all four angles in
// page mode. Sparse-text mode rescues it to seven words at 67%, which is the
// difference between useless and poor, and that is as far as tuning goes.
//
// A scene-text model is built for the other case. The architecture is a pair: a
// detector says where the text is, a recogniser reads each piece it found. That
// split is why it works on a sign - nothing has to look like a page - and it is
// also why it cannot replace Tesseract here, because a detector/recogniser pair
// returns strings and boxes and no structure at all. Tapping a word to grow the
// selection to its paragraph needs paragraphs, and this will not have them. Two
// engines, chosen per photograph.
//
// The plan, which the rest of the project's rules already decide:
//
//   ncnn for the runtime, not ONNX Runtime. ncnn is C++11, has no dependencies,
//   cross-compiles for armv7hl and aarch64 the way Leptonica and Tesseract
//   already do in 3rdparty/, and is a few hundred kilobytes rather than fifteen
//   megabytes. ONNX Runtime wants a toolchain newer than the SDK has.
//
//   PP-OCR mobile for the models: DBNet for detection, about 4.7MB, and SVTR-LCNet
//   for recognition, about 10MB for the Latin set. Downloaded at build time and
//   verified against a pinned SHA256, exactly as the language data is - never
//   committed, always inside the RPM, and still not one outbound request from the
//   app.
//
// What is here is the detector's output stage: DBNet emits a probability per
// pixel, and somebody has to turn that into boxes. That somebody holds all the
// decisions - which pixels count, how far a box is grown past the blob that
// produced it, what is too small to be a word - so by this project's rule it goes
// in a layer plain Qt5 can test, and the ncnn call that produces the map will be
// a thin translation with no decisions in it, the way ocrengine.cpp is for
// Tesseract.
namespace SceneText {

// A piece of text the detector found, in the coordinates of the map.
struct Region
{
    QRect box;

    // Mean probability over the blob, which is how sure the detector is that this
    // is text at all. Not a reading confidence - nothing has been read yet.
    float score = 0.0f;
};

// A DBNet probability map: one float per pixel, row-major, 0 to 1.
//
// Borrowed, not owned. It belongs to the inference runtime's output blob and
// outlives this call.
struct Map
{
    const float *values = nullptr;
    int width = 0;
    int height = 0;

    bool isNull() const { return !values || width <= 0 || height <= 0; }
    float at(int x, int y) const { return values[qint64(y) * width + x]; }
};

// A pixel counts as text above this. DBNet is trained to be confident, so the
// map is close to binary already and the threshold is not delicate; 0.3 is the
// value the paper and every port of it use.
const float MapThreshold = 0.3f;

// A region has to average this much to be kept. Lower than it sounds, because the
// mean is taken over the whole blob including its soft edges.
const float BoxThreshold = 0.5f;

// How far a box is grown past the blob that produced it.
//
// This is not a fudge factor, it is undoing something the training did: DBNet is
// trained on *shrunk* text regions, so that two adjacent words stay two blobs
// instead of merging into one. The detector therefore always reports less than
// the text, and every implementation grows it back by the same rule - the offset
// is the blob's area times this ratio over its perimeter, so a long thin line
// grows by about as much as a short fat one.
const qreal Unclip = 1.5;

// Smaller than this, in pixels on either side, is not a word. Sensor noise in a
// dark frame clears the probability threshold in ones and twos.
const int MinSide = 3;

// The text regions in a probability map, in reading order: top to bottom, then
// left to right within a band.
//
// Reading order rather than by score, because the output is text and text has an
// order. A band is a line's worth of vertical overlap, so two words side by side
// come out side by side rather than sorted by how sure the detector was.
QVector<Region> regionsIn(const Map &map);

// A box measured on the map, in the coordinates of the photograph it came from.
//
// Detectors run at a fixed input size, so the map is almost never the size of the
// photo, and this is the step that quietly ruins an overlay if it is skipped -
// the same trap as ImagePrep::toSourceRect, which is why it is written out and
// pinned by a test rather than done at the call site.
QRect toSourceRect(const QRect &box, const QSize &mapSize, const QSize &sourceSize);

} // namespace SceneText

#endif // SCENETEXT_H
