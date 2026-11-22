import QtQuick
import QtQuick.Layouts
import LynxPlay

// Pairs this PC with a console: Account ID, the code on the TV, then the
// attempt and how it ended.
DialogShell {
    id: dialog

    required property ConsolesController consoles
    readonly property PairingController pairing: dialog.consoles.pairing

    property string consoleKey
    property string consoleName
    // The step the user is on until an attempt starts: account or pin.
    property string step: "account"
    property string pinErrorText

    // What the dialog shows: account, pin, pairing, success or failure.
    readonly property string view: {
        switch (dialog.pairing.state) {
        case PairingController.Pairing:
            return "pairing";
        case PairingController.Succeeded:
            return "success";
        case PairingController.Failed:
            return "failure";
        }

        return dialog.step;
    }
    readonly property var headings: ({
        account: { eyebrow: "Passo 1 de 2", title: `Parear com ${dialog.consoleName}` },
        pin: { eyebrow: "Passo 2 de 2", title: `Parear com ${dialog.consoleName}` },
        pairing: { eyebrow: "Passo 2 de 2", title: `Pareando com ${dialog.consoleName}` },
        success: { eyebrow: "", title: "Console pareado" },
        failure: { eyebrow: "Passo 2 de 2", title: "Não foi possível parear" }
    })
    readonly property var linkDevicePath: [
        "Configurações",
        "Sistema",
        "Reprodução remota",
        "Vincular dispositivo"
    ]

    signal paired(string key, string name)

    function openFor(key, name) {
        dialog.pairing.reset();
        dialog.consoleKey = key;
        dialog.consoleName = name;
        dialog.step = "account";
        dialog.pinErrorText = "";
        accountField.text = dialog.consoles.savedAccountId;
        accountField.errorText = "";
        pinInput.text = "";
        nameField.text = name;
        dialog.open();
    }

    function continueToPin() {
        const error = dialog.pairing.accountIdError(accountField.text);
        accountField.errorText = error;

        if (error.length > 0) {
            accountField.focusInput();
            return;
        }

        dialog.step = "pin";
        pinInput.focusInput();
    }

    function startPairing() {
        dialog.pinErrorText = dialog.pairing.pinError(pinInput.text);

        if (dialog.pinErrorText.length > 0) {
            pinInput.focusInput();
            return;
        }

        dialog.pairing.start(dialog.consoleKey, accountField.text, pinInput.text);
    }

    function goBack() {
        if (dialog.view === "failure") {
            dialog.pairing.reset();
            dialog.step = "pin";
            pinInput.focusInput();
            return;
        }

        dialog.step = "account";
        accountField.focusInput();
    }

    function retry() {
        const step = dialog.pairing.retryStep;

        if (step.length === 0) {
            dialog.pairing.start(dialog.consoleKey, accountField.text, pinInput.text);
            return;
        }

        dialog.pairing.reset();
        dialog.step = step;

        if (step === "pin") {
            pinInput.text = "";
            pinInput.focusInput();
            return;
        }

        accountField.focusInput();
    }

    function finish(playNow) {
        const key = dialog.pairing.pairedKey;
        const name = nameField.text.trim();

        if (name !== dialog.consoleName) {
            dialog.consoles.rename(key, name);
        }

        dialog.close();
        dialog.paired(key, name.length > 0 ? name : dialog.consoleName);

        if (playNow) {
            dialog.consoles.play(key);
        }
    }

    function focusViewAction() {
        switch (dialog.view) {
        case "pairing":
            cancelButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        case "success":
            playButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        case "failure":
            retryButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        }
    }

    eyebrow: dialog.headings[dialog.view].eyebrow
    title: dialog.headings[dialog.view].title
    onViewChanged: dialog.focusViewAction()
    onOpened: accountField.focusInput()
    onClosed: dialog.pairing.cancel()

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "account"
        spacing: Theme.space5

        AppText {
            Layout.fillWidth: true
            text: "Para jogar remotamente, o PS5 precisa reconhecer este PC. Primeiro, informe o Account ID da sua conta PSN."
            textStyle: "bodyMedium"
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }

        InputField {
            id: accountField

            Layout.fillWidth: true
            label: "Account ID da PSN"
            placeholderText: "Ex.: 1234567890123456789"
            helperText: "Aceita o número (19 dígitos) ou o formato Base64, como Kx3fQ9aZ0bM=."
            maximumLength: 24
            onAccepted: dialog.continueToPin()
        }

        LinkButton {
            iconName: "circle-question-mark"
            text: "Como encontrar meu Account ID"
            onClicked: Qt.openUrlExternally("https://chiaki-ng.github.io/chiaki-ng/setup/configuration/#registering-your-playstation")
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: privacyNote.implicitHeight + 2 * Theme.space3
            radius: Theme.radiusMd
            color: Theme.surface

            RowLayout {
                id: privacyNote

                anchors.fill: parent
                anchors.topMargin: Theme.space3
                anchors.bottomMargin: Theme.space3
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: Theme.space3

                Icon {
                    Layout.alignment: Qt.AlignTop
                    name: "lock-keyhole"
                    size: 18
                    color: Theme.textMuted
                }

                AppText {
                    Layout.fillWidth: true
                    text: "O Account ID fica salvo só neste PC e é enviado apenas para o seu PS5 durante o pareamento."
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "pin"
        spacing: Theme.space5

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            AppText {
                Layout.fillWidth: true
                text: "No PS5, gere um código de 8 dígitos:"
                textStyle: "bodyMedium"
                color: Theme.textMuted
            }

            Flow {
                Layout.fillWidth: true
                spacing: 6

                Repeater {
                    model: dialog.linkDevicePath

                    delegate: Row {
                        id: crumb

                        required property int index
                        required property string modelData

                        spacing: 6

                        Rectangle {
                            implicitWidth: crumbLabel.implicitWidth + 20
                            implicitHeight: 28
                            radius: Theme.radiusSm
                            color: Theme.surfaceHigh

                            AppText {
                                id: crumbLabel

                                anchors.centerIn: parent
                                text: crumb.modelData
                                textStyle: "labelMedium"
                            }
                        }

                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: crumb.index < dialog.linkDevicePath.length - 1
                            name: "chevron-right"
                            size: 14
                            color: Theme.textSubtle
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            AppText {
                text: "Código mostrado na TV"
                textStyle: "labelMedium"
            }

            PinInput {
                id: pinInput

                error: dialog.pinErrorText.length > 0
                onAccepted: dialog.startPairing()
                onTextChanged: dialog.pinErrorText = ""
            }

            AppText {
                Layout.fillWidth: true
                text: dialog.pinErrorText.length > 0
                      ? dialog.pinErrorText
                      : "O código expira em poucos minutos. Se sumir da TV, gere outro."
                textStyle: "bodyMedium"
                color: dialog.pinErrorText.length > 0 ? Theme.danger : Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: Theme.space6
        Layout.bottomMargin: Theme.space3
        visible: dialog.view === "pairing"
        spacing: Theme.space4

        Spinner {
            Layout.alignment: Qt.AlignHCenter
        }

        AppText {
            Layout.alignment: Qt.AlignHCenter
            text: "Registrando este PC no console…"
            textStyle: "titleSmall"
        }

        AppText {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 400
            horizontalAlignment: Text.AlignHCenter
            text: "Mantenha o PS5 na tela Vincular dispositivo até terminar. Leva poucos segundos."
            textStyle: "bodyMedium"
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "success"
        spacing: Theme.space5

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space4

            Rectangle {
                implicitWidth: 56
                implicitHeight: 56
                radius: 28
                color: Theme.readySubtle

                Icon {
                    anchors.centerIn: parent
                    name: "check"
                    size: 28
                    bold: true
                    color: Theme.ready
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                AppText {
                    Layout.fillWidth: true
                    text: `Tudo certo com o ${dialog.consoleName}`
                    textStyle: "titleSmall"
                }

                AppText {
                    Layout.fillWidth: true
                    text: "Este PC já pode ligar e jogar neste console."
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                }
            }
        }

        InputField {
            id: nameField

            Layout.fillWidth: true
            label: "Nome do console (opcional)"
            helperText: "Aparece no card. Ex.: PS5 da sala."
            maximumLength: 32
            onAccepted: dialog.finish(false)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "failure"
        spacing: Theme.space5

        Banner {
            Layout.fillWidth: true
            tone: "danger"
            title: dialog.pairing.failureTitle
            message: dialog.pairing.failureMessage
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.pairing.retryStep === "pin"
            spacing: 10

            AppText {
                text: "Código mostrado na TV"
                textStyle: "labelMedium"
            }

            PinInput {
                error: true
                readOnly: true
                text: pinInput.text
            }
        }
    }

    actions: [
        ActionButton {
            id: cancelButton

            visible: dialog.view === "account" || dialog.view === "pairing"
            variant: "ghost"
            text: "Cancelar"
            onClicked: dialog.close()
        },
        ActionButton {
            visible: dialog.view === "pin" || dialog.view === "failure"
            variant: "ghost"
            text: "Voltar"
            onClicked: dialog.goBack()
        },
        ActionButton {
            visible: dialog.view === "success"
            variant: "ghost"
            text: "Concluir"
            onClicked: dialog.finish(false)
        },
        ActionButton {
            visible: dialog.view === "account"
            variant: "primary"
            text: "Continuar"
            onClicked: dialog.continueToPin()
        },
        ActionButton {
            visible: dialog.view === "pin"
            variant: "primary"
            text: "Parear"
            onClicked: dialog.startPairing()
        },
        ActionButton {
            id: playButton

            visible: dialog.view === "success"
            variant: "primary"
            iconName: "play"
            text: "Jogar agora"
            onClicked: dialog.finish(true)
        },
        ActionButton {
            id: retryButton

            visible: dialog.view === "failure"
            variant: "primary"
            iconName: "rotate-ccw"
            text: "Tentar de novo"
            onClicked: dialog.retry()
        }
    ]
}
