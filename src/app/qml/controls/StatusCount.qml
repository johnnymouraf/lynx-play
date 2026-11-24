import QtQuick
import MFPlay

// The design's StatusCount: how many consoles are in one state.
Rectangle {
    id: counter

    // ready, standby or unpaired.
    property string status: "ready"
    property int count: 0
    property string label

    readonly property var dotColors: ({
        ready: Theme.ready,
        standby: Theme.standby,
        unpaired: Theme.info
    })

    implicitWidth: row.implicitWidth + 2 * 14
    implicitHeight: Theme.space10
    radius: Theme.radiusMd
    color: Theme.surface
    Accessible.role: Accessible.StaticText
    Accessible.name: `${counter.count} ${counter.label}`

    Row {
        id: row

        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 8
            height: 8
            radius: 2
            color: counter.dotColors[counter.status]
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: counter.count
            textStyle: "count"
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: counter.label
            textStyle: "bodySmall"
            color: Theme.textMuted
        }
    }
}
