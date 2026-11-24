import QtQuick
import QtQuick.Layouts
import MFPlay

// Gives a console the name its card shows.
DialogShell {
    id: dialog

    property string consoleKey
    property string consoleName

    signal renameRequested(string key, string name)

    function openFor(key, name) {
        dialog.consoleKey = key;
        dialog.consoleName = name;
        nameField.text = name;
        dialog.open();
    }

    function submit() {
        dialog.renameRequested(dialog.consoleKey, nameField.text);
        dialog.close();
    }

    title: "Renomear console"
    onOpened: nameField.focusInput()

    InputField {
        id: nameField

        Layout.fillWidth: true
        label: "Nome do console"
        placeholderText: dialog.consoleName
        helperText: "Aparece no card. Ex.: PS5 da sala. Deixe vazio para usar o nome do PS5."
        maximumLength: 32
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
            text: "Salvar"
            onClicked: dialog.submit()
        }
    ]
}
