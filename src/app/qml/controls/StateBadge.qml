import QtQuick
import MFPlay

// A console's state as a tag: ready, standby, unpaired, offline, error,
// waking or connecting.
Rectangle {
    id: badge

    property string status: "offline"
    // Replaces the status's default text when set.
    property string label: ""

    readonly property var looks: ({
        ready: { icon: "circle-check", color: Theme.ready, label: "Pronto" },
        standby: { icon: "moon", color: Theme.standby, label: "Em repouso" },
        unpaired: { icon: "link-2", color: Theme.info, label: "Não pareado" },
        offline: { icon: "wifi-off", color: Theme.offline, label: "Offline" },
        error: { icon: "circle-exclamation-point", color: Theme.danger, label: "Erro" },
        waking: { icon: "power", color: Theme.standby, label: "Ligando…" },
        connecting: { icon: "loader-circle", color: Theme.info, label: "Conectando…" }
    })
    readonly property var look: looks[status]
    readonly property string displayLabel: badge.label.length > 0 ? badge.label : badge.look.label

    implicitWidth: row.implicitWidth + 2 * Theme.space2
    implicitHeight: 22
    radius: 3
    color: Theme.menu
    Accessible.role: Accessible.StaticText
    Accessible.name: badge.displayLabel

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 6

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: badge.look.icon
            size: 12
            bold: true
            color: badge.look.color
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: badge.displayLabel
            textStyle: "tag"
            color: badge.look.color
        }
    }
}
