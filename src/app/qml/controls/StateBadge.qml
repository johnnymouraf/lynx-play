import QtQuick
import MFPlay

// A console's state as a pill: ready, standby, unpaired, offline, waking or
// error.
Rectangle {
    id: badge

    property string status: "offline"
    // Replaces the status's default text when set.
    property string label: ""

    readonly property var looks: ({
        ready: { icon: "circle-check", color: Theme.ready, fill: Theme.readySubtle, label: "Pronto" },
        standby: { icon: "moon", color: Theme.standby, fill: Theme.standbySubtle, label: "Em repouso" },
        unpaired: { icon: "link-2", color: Theme.info, fill: Theme.infoSubtle, label: "Não pareado" },
        offline: { icon: "wifi-off", color: Theme.offline, fill: Theme.offlineSubtle, label: "Offline" },
        waking: { icon: "power", color: Theme.standby, fill: Theme.standbySubtle, label: "Ligando…" },
        error: { icon: "circle-exclamation-point", color: Theme.danger, fill: Theme.dangerSubtle, label: "Erro" }
    })
    readonly property var look: looks[status]
    readonly property string displayLabel: badge.label.length > 0 ? badge.label : badge.look.label

    implicitWidth: row.implicitWidth + 2 * Theme.space2
    implicitHeight: 28
    radius: Theme.radiusFull
    color: badge.look.fill
    Accessible.role: Accessible.StaticText
    Accessible.name: badge.displayLabel

    Row {
        id: row

        anchors.centerIn: parent
        spacing: Theme.space1

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: badge.look.icon
            size: 14
            bold: true
            color: badge.look.color
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: badge.displayLabel
            textStyle: "caption"
            color: badge.look.color
        }
    }
}
