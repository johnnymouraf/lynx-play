import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// Every shared control in its states, to compare with the design's
// "Design system" page. Opened by the `--gallery` developer option.
Rectangle {
    id: gallery

    color: Theme.bg

    Flickable {
        id: scroller

        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * Theme.space8
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: content

            x: Theme.space8
            y: Theme.space8
            width: scroller.width - 2 * Theme.space8
            spacing: Theme.space6

            GallerySection {
                title: "Button"

                Repeater {
                    model: ["primary", "secondary", "ghost", "danger"]

                    Column {
                        required property string modelData

                        spacing: Theme.space3

                        ActionButton {
                            variant: modelData
                            iconName: "play"
                            text: "Jogar"
                        }

                        ActionButton {
                            variant: modelData
                            iconName: "play"
                            text: "Jogar"
                            enabled: false
                        }
                    }
                }
            }

            GallerySection {
                title: "IconButton, StateBadge"

                IconButton {
                    text: "Fechar"
                }

                Repeater {
                    model: ["ready", "standby", "unpaired", "offline", "error", "waking", "connecting"]

                    StateBadge {
                        required property string modelData

                        status: modelData
                    }
                }
            }

            GallerySection {
                title: "ControllerHint, ControllerStatus"

                Repeater {
                    model: ["cross", "circle", "triangle", "square", "options"]

                    ControllerHint {
                        required property string modelData

                        button: modelData
                        label: "Jogar"
                    }
                }

                ControllerStatus {
                    connected: true
                    name: "DualSense"
                    status: "Conectado"
                    connection: "via Bluetooth"
                }

                ControllerStatus {
                    connected: true
                    name: "DualSense"
                    status: "Bateria 12 %"
                    connection: "via Bluetooth"
                    batteryLow: true
                }

                ControllerStatus {
                    connected: false
                }
            }

            GallerySection {
                title: "TextField"

                InputField {
                    width: 400
                    label: "Endereço IP ou nome do host"
                    placeholderText: "Ex.: 192.168.0.25 ou ps5.local"
                    helperText: "O PC e o PS5 precisam estar na mesma rede."
                }

                InputField {
                    width: 400
                    label: "Endereço IP ou nome do host"
                    text: "192.168.0.300"
                    errorText: "Endereço inválido. Use um IP como 192.168.0.25 ou um nome como ps5.local."
                }
            }

            GallerySection {
                title: "Select, PinInput"

                Select {
                    implicitWidth: 360
                    label: "Saída de áudio"
                    options: [
                        { label: "Padrão do Windows", value: "default" },
                        { label: "Alto-falantes (Realtek Audio)", value: "speakers" },
                        { label: "Fone de ouvido (Arctis 7)", value: "headset" }
                    ]
                    currentValue: "speakers"
                }

                Column {
                    spacing: Theme.space4

                    PinInput {
                        text: "4815"
                    }

                    PinInput {
                        text: "48152906"
                        error: true
                    }
                }
            }

            GallerySection {
                title: "Toggle, SegmentedControl, Slider"

                Toggle {
                    checked: true
                }

                Toggle {
                    checked: false
                }

                Toggle {
                    checked: true
                    enabled: false
                }

                Toggle {
                    checked: false
                    enabled: false
                }

                SegmentedControl {
                    name: "Resolução"
                    options: [
                        { label: "720p", value: 720 },
                        { label: "1080p", value: 1080 },
                        { label: "1440p", value: 1440 }
                    ]
                    currentValue: 1080
                }

                ValueSlider {
                    implicitWidth: 320
                    value: 0.62
                }
            }

            GallerySection {
                title: "NavItem, StatusCount, ViewToggle, Spinner, ScanIndicator"

                NavItem {
                    text: "Consoles"
                    iconName: "gamepad-2"
                }

                NavItem {
                    text: "Consoles"
                    iconName: "gamepad-2"
                    selected: true
                }

                StatusCount {
                    implicitWidth: 226
                    status: "ready"
                    count: 1
                    label: "Pronto"
                }

                StatusCount {
                    implicitWidth: 226
                    status: "standby"
                    count: 1
                    label: "Em repouso"
                }

                StatusCount {
                    implicitWidth: 226
                    status: "unpaired"
                    count: 1
                    label: "Não pareado"
                }

                ViewToggle {}

                Spinner {}

                ScanIndicator {}
            }

            GallerySection {
                title: "SettingRow, DialogShell"

                SettingRow {
                    implicitWidth: 720
                    label: "Resolução"
                    description: "Resolução pedida ao PS5. A imagem final é ampliada para a tela."
                }

                ActionButton {
                    variant: "primary"
                    iconName: "link-2"
                    text: "Abrir diálogo"
                    onClicked: sampleDialog.open()
                }
            }
        }
    }

    DialogShell {
        id: sampleDialog

        iconName: "link-2"
        eyebrow: "Passo 1 de 2"
        title: "Parear console"

        AppText {
            Layout.fillWidth: true
            text: "O corpo do diálogo."
            color: Theme.textMuted
        }

        actions: [
            ActionButton {
                variant: "ghost"
                text: "Cancelar"
                onClicked: sampleDialog.close()
            },
            ActionButton {
                variant: "primary"
                iconName: "check"
                text: "Continuar"
                onClicked: sampleDialog.close()
            }
        ]
    }
}
