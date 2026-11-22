import QtQuick
import QtQuick.Layouts
import LynxPlay

// Settings until their own screens exist: the app's state and the video test
// window.
Item {
    id: page

    required property AppController controller

    property bool videoTestFailed: false

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Theme.space8
        anchors.leftMargin: Theme.space10
        anchors.rightMargin: Theme.space10
        spacing: Theme.space2

        ColumnLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.space3
            spacing: Theme.space1

            AppText {
                text: "Configurações"
                textStyle: "titleLarge"
                Accessible.role: Accessible.Heading
            }

            AppText {
                Layout.fillWidth: true
                text: "Vídeo, áudio e controles ganham suas opções nas próximas versões."
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: appCard.implicitHeight + 2 * Theme.space5
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: 1
            border.color: Theme.border

            RowLayout {
                id: appCard

                anchors.fill: parent
                anchors.margins: Theme.space5
                spacing: Theme.space5

                RoundedImage {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 72
                    Layout.fillWidth: false
                    source: Theme.brandMark
                    sourceWidth: 216
                    topLeftRadius: 18
                    topRightRadius: 18
                    bottomLeftRadius: 18
                    bottomRightRadius: 18
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    AppText {
                        text: "Lynx Play"
                        font.family: Theme.fontDisplay
                        font.pixelSize: 24
                        lineHeight: 32
                    }

                    AppText {
                        text: `Versão ${page.controller.version} · Windows x64`
                        textStyle: "bodyMedium"
                        color: Theme.textMuted
                    }
                }
            }
        }

        Banner {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            visible: !page.controller.remotePlayReady
            tone: "danger"
            title: "O Remote Play não iniciou"
            message: "Os consoles não podem ser encontrados nem pareados. Veja o log em %LOCALAPPDATA%\\LynxPlay\\logs."
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            spacing: Theme.space6

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                AppText {
                    Layout.fillWidth: true
                    text: "Teste de vídeo"
                    textStyle: "labelLarge"
                }

                AppText {
                    Layout.fillWidth: true
                    text: page.videoTestDescription()
                    textStyle: "bodyMedium"
                    color: page.videoTestFailed ? Theme.danger : Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }

            ActionButton {
                iconName: "monitor-play"
                text: "Abrir teste"
                enabled: page.controller.gpuName.length > 0
                onClicked: page.videoTestFailed = !page.controller.openVideoTest()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space4
            implicitHeight: 1
            color: Theme.border
        }
    }

    function videoTestDescription() {
        if (page.videoTestFailed) {
            return "Não foi possível abrir a janela de vídeo. Veja o log.";
        }

        if (page.controller.gpuName.length === 0) {
            return "Nenhuma GPU compatível com Direct3D 12 foi encontrada.";
        }

        return `Abre a janela Direct3D 12 com um padrão animado na ${page.controller.gpuName}.`;
    }
}
