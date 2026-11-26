import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// Configurações › Rede: the bitrate the local stream uses, the account that
// fills the pairing's Account ID and, for internet play, the remote bitrate and
// address.
ColumnLayout {
    id: section

    required property SettingsController settings
    required property ConsolesController consoles

    readonly property var psn: section.consoles.pairing.psnLogin
    readonly property int psnState: section.psn.state
    readonly property bool loggingIn: section.psnState === PsnLoginClient.Waiting
                                      || section.psnState === PsnLoginClient.Exchanging
    readonly property bool exchanging: section.psnState === PsnLoginClient.Exchanging
    readonly property bool loginFailed: section.psnState === PsnLoginClient.Failed
    readonly property string savedAccount: section.consoles.savedAccountId
    readonly property string account: section.psn.accountId.length > 0
                                       ? section.psn.accountId : section.savedAccount
    readonly property bool signedIn: section.account.length > 0

    spacing: Theme.space2

    function startLogin() {
        redirectField.text = "";
        section.psn.start();
    }

    function submitRedirect() {
        section.psn.submitRedirect(redirectField.text);
    }

    function cancelLogin() {
        redirectField.text = "";
        section.psn.reset();
    }

    SectionHeader {
        Layout.fillWidth: true
        title: "Rede"
        subtitle: "Como o MFPlay chega ao PS5 em casa e fora dela."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Bitrate na rede local"
        description: "Usado quando o PC e o PS5 estão na mesma rede."

        Row {
            spacing: Theme.space3

            ValueSlider {
                anchors.verticalCenter: parent.verticalCenter
                from: section.settings.minBitrateMbps
                to: section.settings.maxBitrateMbps
                stepSize: 1
                snapMode: T.Slider.SnapAlways
                value: section.settings.bitrateMbps
                Accessible.name: "Bitrate na rede local"
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
        label: "Bitrate pela internet"
        description: "Usado fora de casa. Baixe se a imagem travar."

        Row {
            spacing: Theme.space3

            ValueSlider {
                anchors.verticalCenter: parent.verticalCenter
                from: section.settings.minBitrateMbps
                to: section.settings.maxBitrateMbps
                stepSize: 1
                snapMode: T.Slider.SnapAlways
                value: section.settings.internetBitrateMbps
                Accessible.name: "Bitrate pela internet"
                onMoved: section.settings.internetBitrateMbps = Math.round(value)
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: 76
                text: `${section.settings.internetBitrateMbps} Mbps`
                textStyle: "monoMedium"
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: accountColumn.implicitHeight + 2 * Theme.space4
        radius: Theme.radiusMd
        color: Theme.surface

        ColumnLayout {
            id: accountColumn

            anchors.fill: parent
            anchors.topMargin: Theme.space4
            anchors.bottomMargin: Theme.space4
            anchors.leftMargin: Theme.space5
            anchors.rightMargin: Theme.space5
            spacing: Theme.space2

            GridLayout {
                Layout.fillWidth: true
                columns: accountColumn.width < 520 ? 1 : 2
                columnSpacing: Theme.space6
                rowSpacing: Theme.space2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppText {
                            text: "Conta PSN"
                            textStyle: "labelLarge"
                        }

                        StatusTag {
                            Layout.alignment: Qt.AlignVCenter
                            iconName: section.signedIn ? "user-check" : "user-x"
                            text: section.signedIn ? "Conectada" : "Não conectada"
                            textColor: section.signedIn ? Theme.ready : Theme.offline
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: section.signedIn
                              ? `Conta ${section.account}. Para jogar fora de casa sem configurar o roteador. Também preenche o Account ID sozinho.`
                              : "Para jogar fora de casa sem configurar o roteador. Também preenche o Account ID sozinho."
                        textStyle: "bodyMedium"
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                    }

                    AppText {
                        Layout.fillWidth: true
                        visible: section.loginFailed
                        text: `${section.psn.failureTitle} ${section.psn.failureMessage}`
                        textStyle: "bodyMedium"
                        color: Theme.danger
                        wrapMode: Text.WordWrap
                    }
                }

                ActionButton {
                    Layout.alignment: accountColumn.width < 520
                                      ? Qt.AlignLeft | Qt.AlignTop : Qt.AlignRight | Qt.AlignTop
                    visible: !section.loggingIn
                    variant: "secondary"
                    iconName: "log-in"
                    text: "Entrar na PSN"
                    onClicked: section.startLogin()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: section.loggingIn
                spacing: Theme.space2

                InputField {
                    id: redirectField

                    Layout.fillWidth: true
                    label: "Link de retorno"
                    placeholderText: "Cole aqui o endereço da PSN"
                    helperText: "Depois de entrar, copie o endereço da página para onde o navegador foi."
                    onAccepted: section.submitRedirect()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    ActionButton {
                        variant: "secondary"
                        iconName: "circle-check"
                        text: "Concluir"
                        enabled: !section.exchanging
                        onClicked: section.submitRedirect()
                    }

                    ActionButton {
                        variant: "ghost"
                        iconName: "x"
                        text: "Cancelar"
                        onClicked: section.cancelLogin()
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Row {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: Theme.space2

                        visible: section.exchanging

                        Spinner {
                            anchors.verticalCenter: parent.verticalCenter
                            size: 18
                        }

                        AppText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Falando com a PSN…"
                            textStyle: "bodyMedium"
                            color: Theme.textMuted
                        }
                    }
                }
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Endereço fora de casa"
        description: "IP da VPN (Tailscale, ZeroTier) ou endereço com redirecionamento de portas. Chega com o jogo pela internet."
        enabled: false
        opacity: 0.5

        InputField {
            width: 300
            enabled: false
            label: ""
            placeholderText: "Ex.: ps5-casa.minha-vpn"
        }
    }
}
