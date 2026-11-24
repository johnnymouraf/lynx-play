import QtQuick
import QtQuick.Layouts
import MFPlay

// A status or error message as plain text: the title in its tone's color
// (info, ready, standby or danger), an optional detail line and an optional
// action at its end.
RowLayout {
    id: banner

    property string tone: "info"
    property string title
    property string message
    property string actionText

    signal actionTriggered

    readonly property var toneColors: ({
        info: Theme.text,
        ready: Theme.ready,
        standby: Theme.standby,
        danger: Theme.danger
    })

    spacing: Theme.space4
    Accessible.role: Accessible.AlertMessage
    Accessible.name: banner.title
    Accessible.description: banner.message

    ColumnLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignVCenter
        spacing: 2

        AppText {
            Layout.fillWidth: true
            text: banner.title
            textStyle: "labelMedium"
            color: banner.toneColors[banner.tone]
            wrapMode: Text.WordWrap
        }

        AppText {
            Layout.fillWidth: true
            visible: banner.message.length > 0
            text: banner.message
            textStyle: "bodyMedium"
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
    }

    ActionButton {
        Layout.alignment: Qt.AlignVCenter
        visible: banner.actionText.length > 0
        text: banner.actionText
        iconName: "refresh-cw"
        onClicked: banner.actionTriggered()
    }
}
