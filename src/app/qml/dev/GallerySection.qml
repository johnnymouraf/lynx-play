import QtQuick
import QtQuick.Layouts
import MFPlay

// A titled row of the gallery.
ColumnLayout {
    id: section

    property string title
    default property alias items: row.data

    Layout.fillWidth: true
    spacing: Theme.space3

    AppText {
        text: section.title
        textStyle: "overline"
        color: Theme.textSubtle
    }

    Flow {
        id: row

        Layout.fillWidth: true
        spacing: Theme.space6
    }
}
