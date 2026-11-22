import QtQuick
import LynxPlay

// One step of connecting to a console: done, under way or still to come.
Row {
    id: step

    property string label
    property string progress: "pending"

    readonly property bool current: step.progress === "current"
    readonly property bool done: step.progress === "done"

    spacing: Theme.space2
    Accessible.role: Accessible.StaticText
    Accessible.name: step.label

    Item {
        anchors.verticalCenter: parent.verticalCenter
        width: 18
        height: 18

        Icon {
            visible: !step.current
            name: step.done ? "circle-check" : "circle"
            size: 18
            color: step.done ? Theme.ready : Theme.textDisabled
        }

        Icon {
            visible: step.current
            name: "loader-circle"
            size: 18
            color: Theme.accent

            RotationAnimator on rotation {
                from: 0
                to: 360
                duration: 1000
                loops: Animation.Infinite
                running: step.current
            }
        }
    }

    AppText {
        anchors.verticalCenter: parent.verticalCenter
        text: step.label
        textStyle: "labelMedium"
        color: step.progress === "pending" ? Theme.textSubtle : Theme.text
    }
}
