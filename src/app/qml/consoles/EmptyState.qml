import QtQuick
import QtQuick.Layouts
import LynxPlay

// Shown while no console has been found or saved: what to check, and the way
// to add one by its address.
ColumnLayout {
    id: empty

    signal addByAddressRequested

    function focusAction() {
        addButton.forceActiveFocus(Qt.TabFocusReason);
    }

    spacing: Theme.space6

    component ChecklistItem: RowLayout {
        id: item

        required property string number
        required property string title
        required property string detail

        Layout.fillWidth: true
        spacing: Theme.space3

        Rectangle {
            Layout.alignment: Qt.AlignTop
            implicitWidth: 28
            implicitHeight: 28
            radius: Theme.radiusFull
            color: Theme.accentSubtle

            AppText {
                anchors.centerIn: parent
                text: item.number
                textStyle: "labelMedium"
                color: Theme.accent
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            spacing: 2

            AppText {
                Layout.fillWidth: true
                text: item.title
                textStyle: "labelMedium"
                wrapMode: Text.WordWrap
            }

            AppText {
                Layout.fillWidth: true
                text: item.detail
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }
    }

    Item {
        Layout.alignment: Qt.AlignHCenter
        implicitWidth: 168
        implicitHeight: 168

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusFull
            color: "transparent"
            border.width: 1
            border.color: Theme.border
        }

        Rectangle {
            anchors.centerIn: parent
            width: 120
            height: 120
            radius: Theme.radiusFull
            color: "transparent"
            border.width: 1
            border.color: Theme.borderStrong
        }

        Rectangle {
            anchors.centerIn: parent
            width: 72
            height: 72
            radius: Theme.radiusFull
            color: Theme.surfaceRaised
            border.width: 1
            border.color: Theme.accent

            Image {
                anchors.centerIn: parent
                width: 24
                height: 24
                sourceSize: Qt.size(24, 24)
                source: Theme.consoleGlyph
            }
        }

        Rectangle {
            x: 132
            y: 30
            width: 10
            height: 10
            radius: Theme.radiusFull
            color: Theme.accent
        }
    }

    ColumnLayout {
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: false
        Layout.preferredWidth: Math.min(560, empty.width)
        spacing: Theme.space2

        AppText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Nenhum console encontrado ainda"
            textStyle: "titleMedium"
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }

        AppText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Continuamos procurando na sua rede. Se o PS5 não aparecer em alguns segundos, confira os itens abaixo."
            textStyle: "bodyLarge"
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
    }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: Math.min(560, empty.width)
        implicitHeight: checklist.implicitHeight + 2 * Theme.space6
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        ColumnLayout {
            id: checklist

            anchors.fill: parent
            anchors.margins: Theme.space6
            spacing: Theme.space4

            ChecklistItem {
                number: "1"
                title: "O PC e o PS5 estão na mesma rede"
                detail: "Os dois conectados ao mesmo roteador, por Wi-Fi ou cabo."
            }

            ChecklistItem {
                number: "2"
                title: "A Reprodução remota está ativada no PS5"
                detail: "Configurações › Sistema › Reprodução remota › Ativar Reprodução remota"
            }

            ChecklistItem {
                number: "3"
                title: "O PS5 pode ser ligado pela rede"
                detail: "Configurações › Sistema › Economia de energia › Recursos disponíveis no modo de repouso › Permanecer conectado à internet"
            }
        }
    }

    ActionButton {
        id: addButton

        Layout.alignment: Qt.AlignHCenter
        variant: "primary"
        iconName: "plus"
        text: "Adicionar pelo IP"
        onClicked: empty.addByAddressRequested()
    }
}
