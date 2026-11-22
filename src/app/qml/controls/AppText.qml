import QtQuick
import LynxPlay

// Text in one of the design's text styles (Theme.typography).
Text {
    property string textStyle: "bodyMedium"
    readonly property var styleSpec: Theme.typography[textStyle]

    color: Theme.text
    font.family: styleSpec.family
    font.pixelSize: styleSpec.pixelSize
    font.weight: styleSpec.weight
    font.letterSpacing: styleSpec.letterSpacing
    lineHeightMode: Text.FixedHeight
    lineHeight: styleSpec.lineHeight
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
}
