import QtQuick
import QtQuick.Layouts
import MFPlay

// The action at the end of a console card: the button its state allows, or
// the progress of turning the console on with a way to cancel it.
Item {
    id: action

    property string status: "offline"
    property bool searching: false
    // An offline console the PSN reaches: its "Jogar" waits for the local
    // network, then plays over the internet.
    property bool overInternet: false
    property string consoleName
    property real wakeProgress: 0

    readonly property var buttons: ({
        ready: { label: "Jogar", icon: "play", variant: "primary" },
        standby: { label: "Ligar e jogar", icon: "power", variant: "primary" },
        unpaired: { label: "Parear", icon: "link-2", variant: "secondary" },
        offline: { label: "Procurar de novo", icon: "refresh-cw", variant: "secondary" },
        waking: { label: "", icon: "", variant: "secondary" }
    })
    readonly property bool internetOffered: action.overInternet && action.status === "offline"
    readonly property var button: action.buttons[action.internetOffered ? "ready" : action.status]
    readonly property bool waking: action.status === "waking"

    signal triggered
    signal cancelRequested

    function restartWakeProgress() {
        if (action.waking) {
            wakeAnimation.restart();
        }
    }

    implicitHeight: Theme.sizeControl
    onStatusChanged: action.restartWakeProgress()
    Component.onCompleted: action.restartWakeProgress()

    // The PS5 takes about 20 s to answer once woken; the bar eases toward
    // 90 % so it never claims to be done before the console answers.
    NumberAnimation {
        id: wakeAnimation

        target: action
        property: "wakeProgress"
        from: 0
        to: 0.9
        duration: 25000
        easing.type: Easing.OutCubic
    }

    ActionButton {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: !action.waking
        enabled: !action.searching
        variant: action.button.variant
        iconName: action.searching ? "loader-circle" : action.button.icon
        text: action.searching ? "Procurando…" : action.button.label
        focusPolicy: Qt.NoFocus
        Accessible.description: action.consoleName
        onClicked: action.triggered()
    }

    RowLayout {
        anchors.fill: parent
        visible: action.waking
        spacing: Theme.space2

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMd
            color: Theme.surfaceHigh
            clip: true

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space2

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "loader-circle"
                    size: 18
                    color: Theme.accent

                    RotationAnimator on rotation {
                        from: 0
                        to: 360
                        duration: 1000
                        loops: Animation.Infinite
                        running: action.waking && action.visible
                    }
                }

                AppText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Ligando o console…"
                    textStyle: "labelMedium"
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: parent.width * action.wakeProgress
                height: 3
                color: Theme.accent
            }
        }

        IconButton {
            Layout.preferredWidth: Theme.sizeControlLg
            Layout.fillHeight: true
            iconName: "x"
            text: `Cancelar ligação de ${action.consoleName}`
            restingFill: Theme.surfaceHigh
            focusPolicy: Qt.NoFocus
            onClicked: action.cancelRequested()
        }
    }
}
