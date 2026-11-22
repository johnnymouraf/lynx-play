import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// Configurações › Vídeo: what the PS5 is asked to stream and how it is shown.
ColumnLayout {
    id: section

    required property SettingsController settings

    spacing: Theme.space2

    SectionHeader {
        Layout.fillWidth: true
        title: "Vídeo"
        subtitle: "Qualidade da imagem pedida ao PS5. Valores mais altos exigem uma rede mais estável."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Resolução"
        description: "Resolução pedida ao PS5. A imagem é ampliada para a sua tela."

        SegmentedControl {
            name: "Resolução"
            options: [
                { label: "1080p", value: 1080 },
                { label: "720p", value: 720 },
                { label: "540p", value: 540 },
                { label: "360p", value: 360 }
            ]
            currentValue: section.settings.resolution
            onActivated: value => section.settings.resolution = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Taxa de quadros"

        SegmentedControl {
            name: "Taxa de quadros"
            options: [
                { label: "60 fps", value: 60 },
                { label: "30 fps", value: 30 }
            ]
            currentValue: section.settings.framesPerSecond
            onActivated: value => section.settings.framesPerSecond = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Codec"
        description: "H.265 entrega mais qualidade no mesmo bitrate."

        SegmentedControl {
            name: "Codec"
            options: [
                { label: "H.265", value: "h265" },
                { label: "H.264", value: "h264" }
            ]
            currentValue: section.settings.codec
            onActivated: value => section.settings.codec = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Bitrate"
        description: "Na rede local, o máximo que o PS5 aceita costuma ser o melhor."

        Row {
            spacing: Theme.space3

            ValueSlider {
                anchors.verticalCenter: parent.verticalCenter
                from: section.settings.minBitrateMbps
                to: section.settings.maxBitrateMbps
                stepSize: 1
                snapMode: T.Slider.SnapAlways
                value: section.settings.bitrateMbps
                Accessible.name: "Bitrate"
                onMoved: section.settings.bitrateMbps = Math.round(value)
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: 76
                text: `${section.settings.bitrateMbps} Mbps`
                textStyle: "monoMedium"
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Sincronização vertical"
        description: "Menor latência pode mostrar cortes na imagem (tearing)."

        SegmentedControl {
            name: "Sincronização vertical"
            options: [
                { label: "Menor latência", value: true },
                { label: "VSync", value: false }
            ]
            currentValue: section.settings.lowestLatency
            onActivated: value => section.settings.lowestLatency = value
        }
    }
}
