import QtQuick
import QtQuick.Layouts
import MFPlay

// Confirms removing a console and its pairing from this PC.
DialogShell {
    id: dialog

    property string consoleKey
    property string consoleName

    signal forgetRequested(string key)

    function openFor(key, name) {
        dialog.consoleKey = key;
        dialog.consoleName = name;
        dialog.open();
    }

    title: `Esquecer ${dialog.consoleName}?`
    iconName: "trash"
    iconTone: "danger"
    onOpened: cancelButton.forceActiveFocus(Qt.TabFocusReason)

    AppText {
        Layout.fillWidth: true
        text: "O MFPlay apaga deste PC o pareamento e o nome desse console. Para jogar nele de novo, será preciso parear outra vez."
        textStyle: "bodyMedium"
        color: Theme.textMuted
        wrapMode: Text.WordWrap
    }

    actions: [
        ActionButton {
            id: cancelButton

            variant: "secondary"
            iconName: "x"
            text: "Cancelar"
            onClicked: dialog.close()
        },
        ActionButton {
            variant: "danger"
            iconName: "trash"
            text: "Esquecer"
            onClicked: {
                dialog.forgetRequested(dialog.consoleKey);
                dialog.close();
            }
        }
    ]
}
