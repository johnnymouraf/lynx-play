import QtQuick
import MFPlay

// The app mark beside the "MFPlay" wordmark and its tagline.
Row {
    spacing: Theme.space3
    Accessible.role: Accessible.StaticText
    Accessible.name: "MFPlay"

    Image {
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 36
        source: Theme.brandMark
        sourceSize.width: width
        sourceSize.height: height
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            text: "<b>MF</b>Play"
            textFormat: Text.StyledText
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: 18
            font.letterSpacing: -0.18
            lineHeight: 20
            lineHeightMode: Text.FixedHeight
        }

        Text {
            text: "REMOTE PS5 SYSTEM"
            color: Theme.textSubtle
            font.family: Theme.fontMono
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 1.5
        }
    }
}
