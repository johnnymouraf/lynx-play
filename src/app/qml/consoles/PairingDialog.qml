import QtQuick
import QtQuick.Layouts
import MFPlay

// Pairs this PC with a console: Account ID, the code on the TV, then the
// attempt and how it ended.
DialogShell {
    id: dialog

    required property ConsolesController consoles
    readonly property PairingController pairing: dialog.consoles.pairing
    readonly property PsnLoginClient psnLogin: dialog.pairing.psnLogin

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
    readonly property string failureText:
        dialog.pairing.failureTitle + ". " + dialog.pairing.failureMessage

    signal paired(string key, string name)

    function openFor(key, name) {
        dialog.pairing.reset();
        dialog.psnLogin.reset();
        dialog.consoleKey = key;
        dialog.consoleName = name;
        dialog.step = "account";
        dialog.pinErrorText = "";
        accountField.text = dialog.consoles.savedAccountId;
        accountField.errorText = "";
        redirectField.text = "";
        redirectField.errorText = "";
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

    function submitRedirect() {
        const text = redirectField.text.trim();

        if (text.length === 0) {
            redirectField.errorText = "Cole o endereço de retorno da PSN.";
            return;
        }

        redirectField.errorText = "";
        dialog.psnLogin.submitRedirect(text);
    }

    function usePsnAccount() {
        accountField.text = dialog.psnLogin.accountId;
        dialog.continueToPin();
    }

    function resetPsn() {
        dialog.psnLogin.reset();
        accountField.text = dialog.consoles.savedAccountId;
        accountField.errorText = "";
        dialog.focusAccountAction();
    }

    function focusAccountAction() {
        switch (dialog.psnLogin.state) {
        case PsnLoginClient.Waiting:
            redirectField.focusInput();
            break;
        case PsnLoginClient.Succeeded:
            usePsnButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        case PsnLoginClient.Failed:
            retryPsnButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        case PsnLoginClient.Exchanging:
            break;
        default:
            psnSignInButton.forceActiveFocus(Qt.TabFocusReason);
            break;
        }
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
        dialog.focusAccountAction();
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

        dialog.focusAccountAction();
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
    iconName: dialog.view === "failure" ? "triangle-alert" : "link"
    iconTone: dialog.view === "failure" ? "danger" : "accent"
    onViewChanged: dialog.focusViewAction()
    onOpened: dialog.focusAccountAction()
    onClosed: dialog.pairing.cancel()

    Connections {
        target: dialog.psnLogin

        function onStateChanged() {
            if (dialog.psnLogin.state === PsnLoginClient.Succeeded) {
                accountField.text = dialog.psnLogin.accountId;
                return;
            }

            if (dialog.psnLogin.state === PsnLoginClient.Waiting) {
                redirectField.focusInput();
                return;
            }

            if (dialog.psnLogin.state === PsnLoginClient.Failed) {
                retryPsnButton.forceActiveFocus(Qt.TabFocusReason);
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "account"
        spacing: Theme.space4

        AppText {
            Layout.fillWidth: true
            text: "Para jogar remotamente, o PS5 precisa reconhecer este PC. Informe o Account ID da sua conta PSN; ele fica salvo só neste PC."
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

        // The browser sign-in keeps its own flow inside step 1, styled with the
        // dialog's buttons; manual entry stays above it.
        RowLayout {
            Layout.fillWidth: true
            visible: dialog.psnLogin.state === PsnLoginClient.Idle
            spacing: Theme.space3

            AppText {
                text: "Conta PSN"
                textStyle: "labelMedium"
            }

            Item {
                Layout.fillWidth: true
            }

            ActionButton {
                id: psnSignInButton

                variant: "secondary"
                iconName: "external-link"
                text: "Entrar com a PSN"
                onClicked: dialog.psnLogin.start()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.psnLogin.state === PsnLoginClient.Waiting
            spacing: Theme.space3

            AppText {
                Layout.fillWidth: true
                text: "No navegador, entre com a sua conta PSN e autorize o acesso. Depois, copie o endereço da página para onde ele foi redirecionado."
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            InputField {
                id: redirectField

                Layout.fillWidth: true
                label: "Endereço de retorno"
                placeholderText: "https://remoteplay.dl.playstation.net/remoteplay/redirect?code=…"
                maximumLength: 1024
                onAccepted: dialog.submitRedirect()
            }

            RowLayout {
                spacing: Theme.space3

                ActionButton {
                    variant: "primary"
                    iconName: "check"
                    text: "Concluir login"
                    onClicked: dialog.submitRedirect()
                }

                ActionButton {
                    variant: "secondary"
                    iconName: "external-link"
                    text: "Abrir o navegador de novo"
                    onClicked: dialog.psnLogin.start()
                }
            }

            ActionButton {
                variant: "secondary"
                iconName: "pencil"
                text: "Prefiro informar manualmente"
                onClicked: dialog.resetPsn()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: dialog.psnLogin.state === PsnLoginClient.Exchanging
            spacing: Theme.space4

            Spinner {
                size: 32
            }

            AppText {
                Layout.fillWidth: true
                text: "Concluindo o login na PSN…"
                textStyle: "titleSmall"
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.psnLogin.state === PsnLoginClient.Failed
            spacing: Theme.space3

            AppText {
                Layout.fillWidth: true
                text: dialog.psnLogin.failureTitle + ". " + dialog.psnLogin.failureMessage
                textStyle: "bodySmall"
                lineHeight: 18
                color: Theme.danger
                wrapMode: Text.WordWrap
            }

            RowLayout {
                spacing: Theme.space3

                ActionButton {
                    id: retryPsnButton

                    variant: "primary"
                    iconName: "rotate-ccw"
                    text: "Tentar de novo"
                    onClicked: dialog.psnLogin.start()
                }

                ActionButton {
                    variant: "secondary"
                    iconName: "pencil"
                    text: "Informar manualmente"
                    onClicked: dialog.resetPsn()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.psnLogin.state === PsnLoginClient.Succeeded
            spacing: Theme.space2

            AppText {
                Layout.fillWidth: true
                text: "Conta PSN conectada"
                textStyle: "labelMedium"
                color: Theme.ready
            }

            AppText {
                Layout.fillWidth: true
                text: `Account ID ${dialog.psnLogin.accountId}`
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            AppText {
                Layout.fillWidth: true
                text: `Acesso válido até ${dialog.psnLogin.expiryText}.`
                textStyle: "bodyMedium"
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.topMargin: Theme.space1
                spacing: Theme.space3

                ActionButton {
                    id: usePsnButton

                    variant: "primary"
                    iconName: "check"
                    text: "Usar este ID"
                    onClicked: dialog.usePsnAccount()
                }

                ActionButton {
                    variant: "secondary"
                    iconName: "pencil"
                    text: "Usar outro Account ID"
                    onClicked: dialog.resetPsn()
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: dialog.view === "pin"
        spacing: Theme.space4

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
        spacing: Theme.space4

        AppText {
            Layout.fillWidth: true
            text: `Tudo certo com ${dialog.consoleName}. Este PC já pode ligar e jogar neste console.`
            textStyle: "bodySmall"
            lineHeight: 18
            color: Theme.ready
            wrapMode: Text.WordWrap
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
        spacing: Theme.space4

        ColumnLayout {
            Layout.fillWidth: true
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

        AppText {
            Layout.fillWidth: true
            text: dialog.failureText
            textStyle: "bodySmall"
            lineHeight: 18
            color: Theme.danger
            wrapMode: Text.WordWrap
        }
    }

    leadingActions: [
        ActionButton {
            visible: dialog.view === "account"
            variant: "secondary"
            iconName: "circle-question-mark"
            text: "Ajuda"
            onClicked: Qt.openUrlExternally("https://chiaki-ng.github.io/chiaki-ng/setup/configuration/#registering-your-playstation")
        }
    ]

    actions: [
        ActionButton {
            id: cancelButton

            visible: dialog.view === "account" || dialog.view === "pairing"
            variant: "secondary"
            iconName: "x"
            text: "Cancelar"
            onClicked: dialog.close()
        },
        ActionButton {
            visible: dialog.view === "pin" || dialog.view === "failure"
            variant: "secondary"
            iconName: "arrow-left"
            text: "Voltar"
            onClicked: dialog.goBack()
        },
        ActionButton {
            visible: dialog.view === "success"
            variant: "secondary"
            iconName: "check"
            text: "Concluir"
            onClicked: dialog.finish(false)
        },
        ActionButton {
            visible: dialog.view === "account"
                     && dialog.psnLogin.state === PsnLoginClient.Idle
            variant: "primary"
            iconName: "arrow-right"
            text: "Continuar"
            onClicked: dialog.continueToPin()
        },
        ActionButton {
            visible: dialog.view === "pin"
            variant: "primary"
            iconName: "link-2"
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
