import QtQuick
import QtQuick.Layouts
import MFPlay

// Shown while no console has been found or saved: the search summary and the
// way to add one by its address, centered in the surface.
Rectangle {
    id: empty

    signal addByAddressRequested

    function focusAction() {
        addButton.forceActiveFocus(Qt.TabFocusReason);
    }

    radius: Theme.radiusMd
    color: Theme.surface

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 2 * Theme.space10)
        spacing: 12

        AppText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Nenhum console encontrado ainda"
            textStyle: "titleMedium"
            font.pixelSize: 24
            font.weight: Font.Bold
            lineHeight: 30
            font.letterSpacing: -0.6
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }

        AppText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Continuamos procurando. Confira se o PS5 está ligado, na mesma rede do PC e com a Reprodução remota ativada."
            textStyle: "bodyMedium"
            lineHeight: 22
            color: Theme.textMuted
            wrapMode: Text.WordWrap
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
}
