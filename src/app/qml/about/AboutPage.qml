import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// Sobre: the version, the licenses, where the logs are, the source code and
// the video test.
Item {
    id: page

    required property AppController controller
    // The page's side margins, set by the shell for the window's width.
    property int sideMargin: Theme.space12

    property bool videoTestFailed: false

    readonly property var chips: [
        { label: `Versão ${page.controller.version}` },
        { label: "Windows x64" },
        { label: "AGPL-3.0" }
    ]
    readonly property int cardColumns: {
        const columns = Math.floor((cards.width + Theme.space2) / (280 + Theme.space2));

        return Math.max(2, Math.min(4, columns));
    }
    readonly property int cardWidth: Math.max(240, Math.floor((cards.width
                                                                - (page.cardColumns - 1)
                                                                  * cards.spacing)
                                                               / page.cardColumns))

    function videoTestDescription() {
        if (page.videoTestFailed) {
            return "Não foi possível abrir a janela de vídeo. Veja o log.";
        }

        if (page.controller.gpuName.length === 0) {
            return "Nenhuma GPU compatível com Direct3D 12 foi encontrada.";
        }

        return `Abre a janela de vídeo com um padrão animado na ${page.controller.gpuName}.`;
    }

    Flickable {
        id: scroller

        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 26 + Theme.space4
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: content

            x: page.sideMargin
            y: 26
            width: scroller.width - 2 * page.sideMargin
            spacing: Theme.space2

            SectionHeader {
                Layout.fillWidth: true
                title: "Sobre"
                subtitle: "Versão, licenças e onde ficam os registros do app."
            }

            AppText {
                Layout.fillWidth: true
                visible: !page.controller.remotePlayReady
                text: "O Remote Play não iniciou: os consoles não podem ser encontrados nem pareados. Veja o log em %LOCALAPPDATA%\\MFPlay\\logs."
                textStyle: "bodyMedium"
                color: Theme.danger
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: appCard.implicitHeight + 2 * 28
                radius: Theme.radiusMd
                color: Theme.surface

                RowLayout {
                    id: appCard

                    anchors.fill: parent
                    anchors.margins: 28
                    spacing: Theme.space6

                    Image {
                        Layout.preferredWidth: 88
                        Layout.preferredHeight: 88
                        source: Theme.brandMark
                        sourceSize.width: 88
                        sourceSize.height: 88
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "<b>MF</b>Play"
                            textFormat: Text.StyledText
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: 32
                            lineHeight: 36
                            lineHeightMode: Text.FixedHeight
                            Accessible.name: "MFPlay"
                        }

                        AppText {
                            text: "REMOTE PS5 SYSTEM"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.letterSpacing: 2
                            lineHeight: 13
                            color: Theme.textSubtle
                        }

                        Row {
                            spacing: 6

                            Repeater {
                                model: page.chips

                                Rectangle {
                                    required property var modelData

                                    width: chipLabel.implicitWidth + 2 * 10
                                    height: 25
                                    radius: Theme.radiusSm
                                    color: Theme.surfaceRaised

                                    AppText {
                                        id: chipLabel

                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.family: Theme.fontMono
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        lineHeight: 15
                                        color: Theme.textMuted
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Flow {
                id: cards

                Layout.fillWidth: true
                spacing: Theme.space2

                LinkCard {
                    width: page.cardWidth
                    title: "Licença"
                    detail: "Software livre sob a AGPL-3.0, baseado no chiaki-ng. NVIDIA, RTX, DLSS e Reflex são marcas da NVIDIA Corporation."
                    iconName: "scroll-text"
                    buttonIcon: "scroll-text"
                    buttonText: "Ver licenças"
                    onClicked: Qt.openUrlExternally(page.controller.licensesUrl)
                }

                LinkCard {
                    width: page.cardWidth
                    title: "Registros (logs)"
                    detail: "%LOCALAPPDATA%\\MFPlay\\logs"
                    monoDetail: true
                    iconName: "file-text"
                    buttonIcon: "folder-open"
                    buttonText: "Abrir pasta"
                    onClicked: Qt.openUrlExternally(page.controller.logsUrl)
                }

                LinkCard {
                    width: page.cardWidth
                    title: "Código-fonte"
                    detail: "github.com/johnnymouraf/mfplay"
                    monoDetail: true
                    iconName: "code-xml"
                    buttonIcon: "external-link"
                    buttonText: "Abrir no navegador"
                    onClicked: Qt.openUrlExternally("https://github.com/johnnymouraf/mfplay")
                }

                LinkCard {
                    width: page.cardWidth
                    title: "Teste de vídeo"
                    detail: page.videoTestDescription()
                    dangerDetail: page.videoTestFailed
                    iconName: "monitor-play"
                    buttonIcon: "monitor-play"
                    buttonText: "Abrir teste"
                    buttonEnabled: page.controller.gpuName.length > 0
                    onClicked: page.videoTestFailed = !page.controller.openVideoTest()
                }
            }
        }
    }

    component LinkCard: Rectangle {
        id: card

        required property string title
        required property string detail
        required property string iconName
        required property string buttonIcon
        required property string buttonText
        property bool monoDetail: false
        property bool dangerDetail: false
        property bool buttonEnabled: true

        signal clicked

        height: 224
        radius: Theme.radiusMd
        color: Theme.surface

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space5
            spacing: Theme.space3

            Rectangle {
                implicitWidth: 40
                implicitHeight: 40
                radius: Theme.radiusMd
                color: Theme.surfaceRaised

                Icon {
                    anchors.centerIn: parent
                    name: card.iconName
                    size: 20
                    color: Theme.text
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space1

                AppText {
                    Layout.fillWidth: true
                    text: card.title
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    lineHeight: 18
                    wrapMode: Text.WordWrap
                }

                AppText {
                    Layout.fillWidth: true
                    text: card.detail
                    font.family: card.monoDetail ? Theme.fontMono : Theme.fontBody
                    font.pixelSize: 12
                    font.weight: Font.Normal
                    lineHeight: 18
                    color: card.dangerDetail ? Theme.danger : Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }

            ActionButton {
                Layout.fillWidth: true
                variant: "secondary"
                iconName: card.buttonIcon
                text: card.buttonText
                enabled: card.buttonEnabled
                onClicked: card.clicked()
            }
        }
    }
}
