import QtQuick
import QtQuick.Layouts
import LynxPlay

// The design's SettingRow: a label, an optional description and the row's
// control at its end, over a divider.
Item {
    id: row

    property string label
    property string description
    default property alias control: controlSlot.data

    implicitHeight: content.implicitHeight + 2 * Theme.space4

    RowLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space6

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            AppText {
                Layout.fillWidth: true
                text: row.label
                textStyle: "labelLarge"
                wrapMode: Text.WordWrap
            }

            AppText {
                Layout.fillWidth: true
                visible: row.description.length > 0
                text: row.description
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }

        Row {
            id: controlSlot

            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space3
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }
}
