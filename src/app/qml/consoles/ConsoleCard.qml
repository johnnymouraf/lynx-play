import QtQuick
import QtQuick.Layouts
import LynxPlay

// The design's ConsoleCard: the console render, its state, address and last
// activity, and the action its state allows.
Item {
    id: card

    required property int index
    required property string key
    required property string name
    required property string address
    required property string status
    required property string activity
    required property bool saved
    required property bool searching
    property bool focused: false
    property real wakeProgress: 0

    readonly property var actions: ({
        ready: { label: "Jogar", icon: "play", variant: "primary" },
        standby: { label: "Ligar e jogar", icon: "power", variant: "primary" },
        unpaired: { label: "Parear", icon: "link-2", variant: "secondary" },
        offline: { label: "Procurar de novo", icon: "refresh-cw", variant: "secondary" },
        waking: { label: "", icon: "", variant: "secondary" }
    })
    readonly property var action: actions[status]
    readonly property var mediaOpacity: ({
        ready: 1,
        standby: 0.5,
        unpaired: 0.35,
        offline: 0.25,
        waking: 0.7
    })

    signal playRequested
    signal wakeRequested
    signal pairRequested
    signal refreshRequested
    signal cancelWakeRequested
    signal menuRequested(Item anchor)

    function triggerPrimary() {
        switch (card.status) {
        case "ready":
            card.playRequested();
            break;
        case "standby":
            card.wakeRequested();
            break;
        case "unpaired":
            card.pairRequested();
            break;
        case "offline":
            card.refreshRequested();
            break;
        }
    }

    function openMenu() {
        card.menuRequested(moreButton);
    }

    implicitWidth: 300
    implicitHeight: 404
    Accessible.role: Accessible.Grouping
    Accessible.name: card.name

    onStatusChanged: card.restartWakeProgress()
    Component.onCompleted: card.restartWakeProgress()

    function restartWakeProgress() {
        if (card.status === "waking") {
            wakeAnimation.restart();
        }
    }

    NumberAnimation {
        id: wakeAnimation

        target: card
        property: "wakeProgress"
        from: 0
        to: 0.9
        duration: 25000
        easing.type: Easing.OutCubic
    }

    HoverHandler {
        id: hover
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: Theme.surface
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 200

            Rectangle {
                anchors.fill: parent
                color: Theme.mediaBackdrop
                topLeftRadius: Theme.radiusLg
                topRightRadius: Theme.radiusLg
            }

            RoundedImage {
                anchors.fill: parent
                opacity: card.mediaOpacity[card.status]
                source: Theme.consoleRender
                sourceWidth: 768
                topLeftRadius: Theme.radiusLg
                topRightRadius: Theme.radiusLg
            }

            StateBadge {
                x: 14
                y: 14
                status: card.status
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space4
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.bottomMargin: Theme.space5
            spacing: Theme.space4

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Image {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    sourceSize: Qt.size(24, 24)
                    source: Theme.consoleGlyph
                }

                AppText {
                    Layout.fillWidth: true
                    text: card.name
                    textStyle: "titleSmall"
                }

                IconButton {
                    id: moreButton

                    iconName: "ellipsis"
                    text: `Opções de ${card.name}`
                    focusPolicy: Qt.NoFocus
                    onClicked: card.openMenu()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Icon {
                        name: "network"
                        size: 16
                        color: Theme.textSubtle
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: card.address
                        textStyle: "monoMedium"
                        color: Theme.textMuted
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Icon {
                        name: "clock"
                        size: 16
                        color: Theme.textSubtle
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: card.activity
                        textStyle: "bodyMedium"
                        color: Theme.textMuted
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.control

                ActionButton {
                    anchors.fill: parent
                    visible: card.status !== "waking"
                    enabled: !card.searching
                    variant: card.action.variant
                    iconName: card.searching ? "loader-circle" : card.action.icon
                    text: card.searching ? "Procurando…" : card.action.label
                    focusPolicy: Qt.NoFocus
                    Accessible.description: card.name
                    onClicked: card.triggerPrimary()
                }

                RowLayout {
                    anchors.fill: parent
                    visible: card.status === "waking"
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
                                    running: card.status === "waking"
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
                            width: parent.width * card.wakeProgress
                            height: 3
                            color: Theme.accent
                        }
                    }

                    IconButton {
                        iconName: "x"
                        text: `Cancelar ligação de ${card.name}`
                        restingFill: Theme.surfaceHigh
                        focusPolicy: Qt.NoFocus
                        onClicked: card.cancelWakeRequested()
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: "transparent"
        border.width: 1
        border.color: hover.hovered ? Theme.borderStrong : Theme.border
    }

    FocusRing {
        targetRadius: Theme.radiusLg
        visible: card.focused
    }
}
