import QtQuick
import LynxPlay

// The keyboard and controller focus ring, drawn around its parent. Set
// `targetRadius` to the parent's corner radius.
Rectangle {
    property real targetRadius: 0

    anchors.fill: parent
    anchors.margins: -3
    radius: targetRadius + 3
    color: "transparent"
    border.width: Theme.focusRingWidth
    border.color: Theme.focus
}
