import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's Slider: an accent track up to a square thumb. With keyboard or
// controller focus the track and the thumb grow, which is its focus state.
T.Slider {
    id: control

    readonly property int trackHeight: control.visualFocus ? 8 : 6
    readonly property int thumbSize: control.visualFocus ? 20 : 16

    implicitWidth: 240
    implicitHeight: Theme.sizeControlSm
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: control.trackHeight

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: Theme.surfaceHover
        }

        Rectangle {
            width: control.handle.x + control.handle.width / 2 - control.leftPadding
            height: parent.height
            radius: 3
            color: control.pressed ? Theme.accentPressed : Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.thumbSize
        height: control.thumbSize
        radius: 4
        color: Theme.text
    }
}
