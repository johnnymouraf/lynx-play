import QtQuick
import QtQuick.Layouts
import LynxPlay

// Adds a console the network search can't find, by its address.
DialogShell {
    id: dialog

    required property ConsolesController consoles

    signal added(string key)

    function openEmpty() {
        addressField.text = "";
        addressField.errorText = "";
        dialog.open();
    }

    function submit() {
        const error = dialog.consoles.addressError(addressField.text);
        addressField.errorText = error;

        if (error.length > 0) {
            addressField.focusInput();
            return;
        }

        const key = dialog.consoles.addByAddress(addressField.text);
        dialog.close();
        dialog.added(key);
    }

    title: "Adicionar console pelo IP"
    onOpened: addressField.focusInput()

    AppText {
        Layout.fillWidth: true
        text: "Use quando o PS5 não aparece sozinho na lista: em outra sub-rede, por VPN ou com a descoberta bloqueada pelo roteador."
        textStyle: "bodyMedium"
        color: Theme.textMuted
        wrapMode: Text.WordWrap
    }

    InputField {
        id: addressField

        Layout.fillWidth: true
        label: "Endereço IP ou nome do host"
        placeholderText: "Ex.: 192.168.0.25 ou ps5.local"
        helperText: "Aceita IPv4 (192.168.0.25) ou nome do host (ps5.local)."
        maximumLength: 253
        onAccepted: dialog.submit()
    }

    actions: [
        ActionButton {
            variant: "ghost"
            text: "Cancelar"
            onClicked: dialog.close()
        },
        ActionButton {
            variant: "primary"
            text: "Adicionar"
            onClicked: dialog.submit()
        }
    ]
}
