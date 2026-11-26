import QtQuick
import QtQuick.Layouts
import MFPlay

// The immersive screen while connecting to a console, and when that fails.
FocusScope {
    id: page

    required property StreamController stream
    required property AppController controller

    // The error screen asks to sign in to the PSN again.
    signal signInRequested

    readonly property bool failed: page.stream.phase === StreamController.Failed
    readonly property bool startingVideo: page.stream.phase === StreamController.StartingVideo
    readonly property string connectingHeading: page.stream.registering
                                                ? "Pareando pela internet…"
                                                : "Conectando…"

    function focusAction() {
        if (page.failed) {
            if (page.stream.signInNeeded) {
                signInButton.forceActiveFocus(Qt.TabFocusReason);
            } else {
                retryButton.forceActiveFocus(Qt.TabFocusReason);
            }

            return;
        }

        cancelButton.forceActiveFocus(Qt.TabFocusReason);
    }

    onFailedChanged: {
        if (page.visible) {
            page.focusAction();
        }
    }

    onVisibleChanged: {
        if (page.visible) {
            page.focusAction();
        }
    }

    Keys.onEscapePressed: page.stream.cancel()

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    // Keeps clicks from reaching the screen underneath.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space6
        anchors.bottomMargin: Theme.space6
        anchors.leftMargin: Theme.space10
        anchors.rightMargin: Theme.space10
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            BrandLogo {}

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                implicitWidth: route.implicitWidth + 2 * Theme.space3
                implicitHeight: 32
                radius: Theme.radiusMd
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Row {
                    id: route

                    anchors.centerIn: parent
                    spacing: Theme.space2

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: page.stream.overInternet ? "network" : "house-wifi"
                        size: 16
                        color: Theme.textMuted
                    }

                    AppText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.stream.overInternet ? "Internet" : "Rede local"
                        textStyle: "caption"
                        color: Theme.textMuted
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width, 640)
                spacing: Theme.space6

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Theme.space2

                    AppText {
                        Layout.alignment: Qt.AlignHCenter
                        text: page.stream.consoleName
                        textStyle: "overline"
                        font.capitalization: Font.AllUppercase
                        color: Theme.textMuted
                    }

                    AppText {
                        Layout.alignment: Qt.AlignHCenter
                        text: page.failed ? page.stream.failureHeading : page.connectingHeading
                        textStyle: "display"
                        Accessible.role: Accessible.Heading
                        Accessible.name: text
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 520
                    visible: !page.failed
                    spacing: Theme.space4

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 6
                        radius: 3
                        color: Theme.surfaceHigh

                        Rectangle {
                            width: parent.width * (page.startingVideo ? 0.85 : 0.58)
                            height: parent.height
                            radius: 3
                            color: Theme.accent

                            Behavior on width {
                                NumberAnimation {
                                    duration: 400
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }

                    AppText {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.stream.connectionNote.length > 0
                        text: page.stream.connectionNote
                        textStyle: "labelMedium"
                        color: Theme.text
                    }

                    AppText {
                        Layout.alignment: Qt.AlignHCenter
                        visible: page.stream.overInternet
                        text: page.stream.connectionStageText
                        textStyle: "bodyMedium"
                        color: Theme.textMuted
                    }

                    Row {
                        Layout.alignment: Qt.AlignHCenter
                        visible: !page.stream.overInternet
                        spacing: Theme.space6

                        ConnectionStep {
                            label: "Console ligado"
                            progress: "done"
                        }

                        ConnectionStep {
                            label: "Conectando"
                            progress: page.startingVideo ? "done" : "current"
                        }

                        ConnectionStep {
                            label: "Iniciando o vídeo"
                            progress: page.startingVideo ? "current" : "pending"
                        }
                    }
                }

                AppText {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 560
                    visible: page.failed
                    text: `${page.stream.failureTitle}. ${page.stream.failureMessage}`
                    textStyle: "bodyMedium"
                    color: Theme.danger
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                ActionButton {
                    id: cancelButton

                    Layout.alignment: Qt.AlignHCenter
                    visible: !page.failed
                    variant: "secondary"
                    iconName: "x"
                    text: "Cancelar"
                    onClicked: page.stream.cancel()
                }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.failed
                    spacing: Theme.space3

                    ActionButton {
                        variant: "secondary"
                        iconName: "arrow-left"
                        text: "Voltar aos consoles"
                        onClicked: page.stream.cancel()
                    }

                    ActionButton {
                        variant: "secondary"
                        iconName: "folder-open"
                        text: "Abrir pasta de logs"
                        onClicked: Qt.openUrlExternally(page.controller.logsUrl)
                    }

                    ActionButton {
                        id: retryButton

                        visible: !page.stream.signInNeeded
                        variant: "primary"
                        iconName: "rotate-ccw"
                        text: "Tentar de novo"
                        onClicked: page.stream.retry()
                    }

                    ActionButton {
                        id: signInButton

                        visible: page.stream.signInNeeded
                        variant: "primary"
                        iconName: "log-in"
                        text: "Entrar de novo"
                        onClicked: page.signInRequested()
                    }
                }
            }
        }
    }
}
