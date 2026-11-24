import QtQuick
import QtQuick.Layouts
import MFPlay

// A settings section's title and what it covers.
ColumnLayout {
    id: header

    property string title
    property string subtitle

    spacing: Theme.space1

    AppText {
        text: header.title
        textStyle: "titleLarge"
        Accessible.role: Accessible.Heading
    }

    AppText {
        Layout.fillWidth: true
        Layout.bottomMargin: Theme.space3
        text: header.subtitle
        textStyle: "bodyMedium"
        color: Theme.textMuted
        wrapMode: Text.WordWrap
    }
}
