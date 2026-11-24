import QtQuick
import MFPlay

// The keyboard and controller focus ring: the design's 2 px outside stroke,
// drawn around its parent. Set `targetRadius` to the parent's corner radius.
Rectangle {
    property real targetRadius: 0

    anchors.fill: parent
    anchors.margins: -Theme.sizeFocusRing
    radius: targetRadius + Theme.sizeFocusRing
    color: "transparent"
    border.width: Theme.sizeFocusRing
    border.color: Theme.focus
}
