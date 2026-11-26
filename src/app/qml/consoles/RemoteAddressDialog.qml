import QtQuick
import QtQuick.Layouts
import MFPlay

// Sets the address a paired console is reached at away from home: a VPN
// address or a port-forwarded one. An empty field removes it.
DialogShell {
    id: dialog

    required property ConsolesController consoles

    property string consoleKey
    property string consoleName

    function openFor(key, name) {
        dialog.consoleKey = key;
        dialog.consoleName = name;
        addressField.text = dialog.consoles.remoteAddressOf(key);
        addressField.errorText = "";
        dialog.open();
    }

    function submit() {
        const text = addressField.text.trim();
        // An empty field removes the address, so only a typed one is checked.
        const error = text.length > 0 ? dialog.consoles.addressError(text) : "";
        addressField.errorText = error;

        if (error.length > 0) {
            addressField.focusInput();
            return;
        }

        dialog.consoles.setRemoteAddress(dialog.consoleKey, text);
        dialog.close();
    }

    title: "Endereço fora de casa"
    iconName: "network"
    onOpened: addressField.focusInput()

    AppText {
        Layout.fillWidth: true
        text: dialog.consoleName + ": use quando o PS5 estiver longe de casa, com o IP "
              + "de uma VPN (Tailscale, ZeroTier) ou um endereço com redirecionamento "
              + "de portas. O MFPlay tenta esse endereço antes da PSN."
        textStyle: "bodyMedium"
        color: Theme.textMuted
        wrapMode: Text.WordWrap
    }

    InputField {
        id: addressField

        Layout.fillWidth: true
        label: "Endereço IP ou nome do host"
        placeholderText: "Ex.: 100.64.0.25 ou ps5-casa.ddns.net"
        helperText: "Deixe vazio para remover. Aceita IPv4 (100.64.0.25) ou nome do host."
        maximumLength: 253
        onAccepted: dialog.submit()
    }

    actions: [
        ActionButton {
            variant: "secondary"
            iconName: "x"
            text: "Cancelar"
            onClicked: dialog.close()
        },
        ActionButton {
            variant: "primary"
            iconName: "check"
            text: "Salvar"
            onClicked: dialog.submit()
        }
    ]
}
