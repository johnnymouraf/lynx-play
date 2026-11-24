import QtQuick
import MFPlay

// The design's ControllerHint: a DualSense button in its color and what it
// does on this screen.
Row {
    id: hint

    // cross, circle, triangle, square or options.
    property string button: "cross"
    property string label

    readonly property var glyphs: ({
        cross: { icon: "x", color: Theme.psCross },
        circle: { icon: "circle", color: Theme.psCircle },
        triangle: { icon: "triangle", color: Theme.psTriangle },
        square: { icon: "square", color: Theme.psSquare },
        options: { icon: "menu", color: Theme.text }
    })
    readonly property var glyph: glyphs[button]

    spacing: Theme.space2

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 26
        height: 26
        radius: Theme.radiusFull
        color: Theme.surfaceHover

        Icon {
            anchors.centerIn: parent
            name: hint.glyph.icon
            size: 14
            bold: true
            color: hint.glyph.color
        }
    }

    AppText {
        anchors.verticalCenter: parent.verticalCenter
        text: hint.label
        textStyle: "bodyMedium"
        color: Theme.textMuted
    }
}
