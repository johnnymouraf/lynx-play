import QtQuick
import QtQuick.Layouts
import MFPlay

// Configurações › Sobre: the version, the licenses, where the logs are, the
// source code and the video test.
ColumnLayout {
    id: section

    required property AppController controller

    property bool videoTestFailed: false

    function videoTestDescription() {
        if (section.videoTestFailed) {
            return "Não foi possível abrir a janela de vídeo. Veja o log.";
        }

        if (section.controller.gpuName.length === 0) {
            return "Nenhuma GPU compatível com Direct3D 12 foi encontrada.";
        }

        return `Abre a janela de vídeo com um padrão animado na ${section.controller.gpuName}.`;
    }

    spacing: Theme.space2

    SectionHeader {
        Layout.fillWidth: true
        title: "Sobre"
        subtitle: "Versão, licenças e onde ficam os registros do app."
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
                    Layout.fillWidth: true
                    text: `<b>MF</b><font color="${Theme.accent}">Play</font>`
                    textFormat: Text.StyledText
                    font.family: Theme.fontDisplay
                    font.pixelSize: 24
                    lineHeight: 32
                    Accessible.name: "MFPlay"
                }

                AppText {
                    text: `Versão ${section.controller.version} · Windows x64`
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                }
            }
        }
    }

    Banner {
        Layout.fillWidth: true
        Layout.topMargin: Theme.space2
        visible: !section.controller.remotePlayReady
        tone: "danger"
        title: "O Remote Play não iniciou"
        message: "Os consoles não podem ser encontrados nem pareados. Veja o log em %LOCALAPPDATA%\MFPlay\logs."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Licença"
        description: "MFPlay é software livre sob a AGPL-3.0, baseado no chiaki-ng. "
                     + "NVIDIA, RTX, DLSS e Reflex são marcas da NVIDIA Corporation."

        LinkButton {
            iconName: "scroll-text"
            text: "Ver licenças de terceiros"
            onClicked: Qt.openUrlExternally(section.controller.licensesUrl)
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Registros (logs)"
        description: "%LOCALAPPDATA%\MFPlay\logs"

        ActionButton {
            iconName: "folder-open"
            text: "Abrir pasta"
            onClicked: Qt.openUrlExternally(section.controller.logsUrl)
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Código-fonte"
        description: "github.com/johnnymouraf/mfplay"

        LinkButton {
            iconName: "external-link"
            text: "Abrir no navegador"
            onClicked: Qt.openUrlExternally("https://github.com/johnnymouraf/mfplay")
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Teste de vídeo"
        description: section.videoTestDescription()

        ActionButton {
            iconName: "monitor-play"
            text: "Abrir teste"
            enabled: section.controller.gpuName.length > 0
            onClicked: section.videoTestFailed = !section.controller.openVideoTest()
        }
    }
}
