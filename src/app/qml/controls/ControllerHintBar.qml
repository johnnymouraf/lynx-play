import QtQuick
import QtQuick.Layouts
import MFPlay

// The design's controller hints: the buttons and what they do on the current
// screen, in a block along the bottom of the content.
Rectangle {
    id: bar

    // {button, label} pairs; see ControllerHint for the buttons.
    property var hints: []

    implicitHeight: Theme.sizeControlLg
    radius: Theme.radiusMd
    color: Theme.surface
    clip: true
    Accessible.role: Accessible.StaticText
    Accessible.name: bar.hints.map(hint => hint.label).join(", ")

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: Theme.space6

        Repeater {
            model: bar.hints

            ControllerHint {
                required property var modelData

                button: modelData.button
                label: modelData.label
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
