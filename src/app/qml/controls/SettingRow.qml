import QtQuick
import QtQuick.Layouts
import MFPlay

// The design's SettingRow: a solid block with a label, an optional
// description and the row's control at its end. A narrow row puts the control
// under the text.
Rectangle {
    id: row

    property string label
    property string description
    default property alias control: controlSlot.data

    readonly property bool stacked: row.width < 560

    implicitHeight: content.implicitHeight + 2 * Theme.space4
    radius: Theme.radiusMd
    color: Theme.surface

    GridLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space5
        anchors.rightMargin: Theme.space5
        anchors.verticalCenter: parent.verticalCenter
        columns: row.stacked ? 1 : 2
        columnSpacing: Theme.space6
        rowSpacing: Theme.space2

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

            Layout.alignment: row.stacked
                              ? Qt.AlignLeft | Qt.AlignVCenter
                              : Qt.AlignRight | Qt.AlignVCenter
            spacing: Theme.space3
        }
    }
}
