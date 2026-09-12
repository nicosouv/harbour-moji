import QtQuick 2.0
import Sailfish.Silica 1.0
import Mochi 1.0

// A hairline separator with a barely perceptible curve, as in the webOS Mochi
// mockups.
//
// Drawn by a fragment shader rather than a Canvas: the curve is a uniform, not
// a bitmap, so a long ListView allocates no texture per delegate. This matters
// on Qt 5.6, where one Canvas per row is a measurable cost at creation time.
//
// Usage inside a delegate:
//   CurvedSeparator { width: parent.width; index: model.index }
ShaderEffect {
    id: root

    // Pass the delegate's index so a row always draws the same curve, even
    // after the view recycles its delegates. The golden angle spreads
    // consecutive rows apart instead of repeating every few rows.
    property int index: 0
    property real phase: (root.index * 2.39996323) % (2 * Math.PI)

    // One full wave across the width reads as a single gentle bow. Raise it
    // and the line starts to look like a decorative squiggle.
    property real waves: 1.0

    property real amplitude: Tokens.curveAmplitude
    property real strokeWidth: Tokens.hairline
    property color lineColor: Tokens.separatorColor

    // Fraction of the width over which the line fades in at each end, so it
    // never butts up against a panel edge.
    property real endFade: 0.06

    implicitHeight: 2 * (amplitude + strokeWidth) + 2

    // Normalised uniforms: the shader works in texture coordinates, so every
    // pixel value has to be divided by the item height.
    readonly property real uAmp: height > 0 ? amplitude / height : 0
    readonly property real uHalf: height > 0 ? strokeWidth / (2 * height) : 0
    readonly property real uFeather: height > 0 ? 1.0 / height : 0
    readonly property real uFreq: waves * 2 * Math.PI

    // Colours reaching a shader are premultiplied by Qt, so multiplying the
    // whole vec4 by the coverage is correct.
    fragmentShader: "
        varying highp vec2 qt_TexCoord0;
        uniform lowp float qt_Opacity;
        uniform lowp vec4 lineColor;
        uniform highp float uAmp;
        uniform highp float uHalf;
        uniform highp float uFeather;
        uniform highp float uFreq;
        uniform highp float phase;
        uniform highp float endFade;

        void main() {
            highp float centre = 0.5 + uAmp * sin(phase + qt_TexCoord0.x * uFreq);
            highp float d = abs(qt_TexCoord0.y - centre);
            lowp float coverage = 1.0 - smoothstep(uHalf, uHalf + uFeather, d);
            coverage *= smoothstep(0.0, endFade, qt_TexCoord0.x);
            coverage *= smoothstep(0.0, endFade, 1.0 - qt_TexCoord0.x);
            gl_FragColor = lineColor * coverage * qt_Opacity;
        }
    "
}
