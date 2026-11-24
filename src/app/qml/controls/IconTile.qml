import QtQuick
import MFPlay

// The square tile that holds an icon at the start of buttons, rail entries
// and cards.
Rectangle {
    id: tile

    property string iconName
    property color iconColor: Theme.text
    property int iconSize: 17

    implicitWidth: 32
    implicitHeight: 32
    radius: Theme.radiusTile

    Icon {
        anchors.centerIn: parent
        name: tile.iconName
        size: tile.iconSize
        color: tile.iconColor
    }
}
