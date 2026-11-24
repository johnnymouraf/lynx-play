import QtQuick
import MFPlay

// A short state label: the design's uppercase tag with an optional icon,
// placed after the title it qualifies.
Rectangle {
    id: tag

    property string iconName
    property string text
    property color color: Theme.standby

    implicitWidth: content.implicitWidth + 2 * Theme.space2
    implicitHeight: 22
    radius: 3
    color: Theme.menu
    Accessible.role: Accessible.StaticText
    Accessible.name: tag.text

    Row {
        id: content

        anchors.centerIn: parent
        spacing: 6

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: tag.iconName.length > 0
            name: tag.iconName
            size: 12
            bold: true
            color: tag.color
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: tag.text
            textStyle: "tag"
            color: tag.color
        }
    }
}
