import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's 4 px scroll bar, shown only when the content overflows.
T.ScrollBar {
    id: control

    implicitWidth: 4
    implicitHeight: 4
    padding: 0
    minimumSize: 0.1
    visible: control.size < 1.0

    contentItem: Rectangle {
        implicitWidth: 4
        implicitHeight: 4
        radius: Theme.radiusFull
        color: control.pressed ? Theme.borderControl : Theme.borderStrong
    }
}
