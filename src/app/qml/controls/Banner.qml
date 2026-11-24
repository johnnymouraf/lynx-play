import QtQuick
import QtQuick.Layouts
import MFPlay

// A message with a tone (info, ready, standby or danger), an optional detail
// line and an optional action at its end.
Rectangle {
    id: banner

    property string tone: "info"
    property string title
    property string message
    property string actionText

    signal actionTriggered

    readonly property var looks: ({
        info: { icon: "info", color: Theme.info, fill: Theme.infoSubtle },
        ready: { icon: "circle-check", color: Theme.ready, fill: Theme.readySubtle },
        standby: { icon: "moon", color: Theme.standby, fill: Theme.standbySubtle },
        danger: { icon: "circle-exclamation-point", color: Theme.danger, fill: Theme.dangerSubtle }
    })
    readonly property var look: looks[tone]

    implicitWidth: 480
    implicitHeight: content.implicitHeight + 2 * Theme.space3
    radius: Theme.radiusMd
    color: banner.look.fill
    Accessible.role: Accessible.AlertMessage
    Accessible.name: banner.title
    Accessible.description: banner.message

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        anchors.topMargin: Theme.space3
        anchors.bottomMargin: Theme.space3
        spacing: Theme.space3

        Icon {
            Layout.alignment: Qt.AlignTop
            name: banner.look.icon
            color: banner.look.color
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 2

            AppText {
                Layout.fillWidth: true
                text: banner.title
                textStyle: "labelMedium"
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

        LinkButton {
            Layout.alignment: Qt.AlignVCenter
            visible: banner.actionText.length > 0
            text: banner.actionText
            iconName: "refresh-cw"
            onClicked: banner.actionTriggered()
        }
    }
}
