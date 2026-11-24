import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's Slider: an accent track up to an amber-rimmed knob, with a
// focus ring around the knob.
T.Slider {
    id: control

    readonly property int knobSize: 22
    readonly property int ringSize: 34

    implicitWidth: 240
    implicitHeight: 36
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 6

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: Theme.surfaceHover
        }

        Rectangle {
            width: control.handle.x + control.handle.width / 2 - control.leftPadding
            height: parent.height
            radius: 3
            color: Theme.accent
        }
    }

    handle: Item {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.ringSize
        height: control.ringSize

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "transparent"
            border.width: Theme.focusRingWidth
            border.color: Theme.focus
            visible: control.visualFocus
        }

        Rectangle {
            anchors.centerIn: parent
            width: control.knobSize
            height: control.knobSize
            radius: width / 2
            color: Theme.text
            border.width: 2
            border.color: control.pressed ? Theme.accentPressed : Theme.accent
        }
    }
}
