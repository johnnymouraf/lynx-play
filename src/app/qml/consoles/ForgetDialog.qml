import QtQuick
import QtQuick.Layouts
import LynxPlay

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
    onOpened: cancelButton.forceActiveFocus(Qt.TabFocusReason)

    AppText {
        Layout.fillWidth: true
        text: "O Lynx Play apaga deste PC o pareamento e o nome desse console. Para jogar nele de novo, será preciso parear outra vez."
        textStyle: "bodyMedium"
        color: Theme.textMuted
        wrapMode: Text.WordWrap
    }

    actions: [
        ActionButton {
            id: cancelButton

            variant: "ghost"
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
