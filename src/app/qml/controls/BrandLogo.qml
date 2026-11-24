import QtQuick
import MFPlay

// The app mark and the "MFPlay" wordmark.
Row {
    spacing: Theme.space3
    Accessible.role: Accessible.StaticText
    Accessible.name: "MFPlay"

    RoundedImage {
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 36
        source: Theme.brandMark
        sourceWidth: 144
        topLeftRadius: 9
        topRightRadius: 9
        bottomLeftRadius: 9
        bottomRightRadius: 9
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter

        Text {
            text: "MF"
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 20
            font.weight: Font.Bold
            font.letterSpacing: -0.2
        }

        Text {
            text: "Play"
            color: Theme.accent
            font.family: Theme.fontDisplay
            font.pixelSize: 20
            font.weight: Font.Normal
            font.letterSpacing: -0.2
        }
    }
}
