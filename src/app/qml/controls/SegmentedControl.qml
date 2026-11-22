import QtQuick
import LynxPlay

// The design's SegmentedControl: one choice among a few. `options` lists
// {label, value}; choosing one emits `activated` with its value.
Rectangle {
    id: control

    property string name
    property var options: []
    property var currentValue

    signal activated(var value)

    implicitWidth: segments.implicitWidth + 2 * Theme.space1
    implicitHeight: segments.implicitHeight + 2 * Theme.space1
    radius: Theme.radiusMd
    color: Theme.bg
    border.width: 1
    border.color: Theme.border
    Accessible.role: Accessible.Grouping
    Accessible.name: control.name

    Row {
        id: segments

        anchors.centerIn: parent
        spacing: Theme.space1

        Repeater {
            model: control.options

            SegmentItem {
                required property var modelData

                text: modelData.label
                selected: modelData.value === control.currentValue
                optionEnabled: modelData.enabled !== undefined ? modelData.enabled : true
                onClicked: control.activated(modelData.value)
            }
        }
    }
}
