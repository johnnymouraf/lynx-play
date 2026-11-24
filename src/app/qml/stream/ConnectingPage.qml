import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import MFPlay

// The immersive screen while connecting to a console, and when that fails.
FocusScope {
    id: page

    required property StreamController stream
    required property AppController controller

    readonly property bool failed: page.stream.phase === StreamController.Failed
    readonly property bool startingVideo: page.stream.phase === StreamController.StartingVideo

    function focusAction() {
        if (page.failed) {
            retryButton.forceActiveFocus(Qt.TabFocusReason);
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

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeWidth: 0
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: page.width / 2
                centerY: page.height / 2
                centerRadius: Math.hypot(page.width, page.height) / 2
                focalX: page.width / 2
                focalY: page.height / 2

                GradientStop {
                    position: 0
                    color: "#1FF5A23A"
                }

                GradientStop {
                    position: 1
                    color: "#00F5A23A"
                }
            }

            PathRectangle {
                width: page.width
                height: page.height
            }
        }
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
                implicitHeight: route.implicitHeight + 2 * Theme.space1
                radius: Theme.radiusFull
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Row {
                    id: route

                    anchors.centerIn: parent
                    spacing: Theme.space2

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "house-wifi"
                        size: 16
                        color: Theme.textMuted
                    }

                    AppText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Rede local"
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
                width: Math.min(parent.width, 560)
                spacing: Theme.space6

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 420
                    implicitHeight: 280
                    radius: Theme.radiusXl
                    color: Theme.mediaBackdrop

                    RoundedImage {
                        anchors.fill: parent
                        opacity: page.failed ? 0.35 : 1
                        source: Theme.consoleRender
                        sourceWidth: 840
                        topLeftRadius: Theme.radiusXl
                        topRightRadius: Theme.radiusXl
                        bottomLeftRadius: Theme.radiusXl
                        bottomRightRadius: Theme.radiusXl
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusXl
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.border
                    }
                }

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
                        text: page.failed ? page.stream.failureHeading : "Conectando…"
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
                            width: parent.width * (page.startingVideo ? 0.85 : 0.55)
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

                    Row {
                        Layout.alignment: Qt.AlignHCenter
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

                ActionButton {
                    id: cancelButton

                    Layout.alignment: Qt.AlignHCenter
                    visible: !page.failed
                    text: "Cancelar"
                    onClicked: page.stream.cancel()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: page.failed
                    spacing: Theme.space2

                    Banner {
                        Layout.fillWidth: true
                        tone: "danger"
                        title: page.stream.failureTitle
                        message: page.stream.failureMessage
                    }

                    LinkButton {
                        Layout.alignment: Qt.AlignHCenter
                        iconName: "folder-open"
                        text: "Abrir pasta de logs"
                        onClicked: Qt.openUrlExternally(page.controller.logsUrl)
                    }
                }

                Row {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.failed
                    spacing: Theme.space3

                    ActionButton {
                        variant: "ghost"
                        text: "Voltar aos consoles"
                        onClicked: page.stream.cancel()
                    }

                    ActionButton {
                        id: retryButton

                        variant: "primary"
                        iconName: "rotate-ccw"
                        text: "Tentar de novo"
                        onClicked: page.stream.retry()
                    }
                }
            }
        }
    }
}
