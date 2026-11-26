import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// The design's ConsoleCard: the console's state, name and address, and the
// action its state allows. In the list view it lays out as one row and adds
// the last activity, which the grid's card has no room for.
Item {
    id: card

    required property int index
    required property string key
    required property string name
    required property string address
    required property string status
    required property string activity
    required property bool saved
    required property bool paired
    required property bool psnReachable
    required property bool searching
    property bool listMode: false
    property bool focused: false
    property bool psnSignedIn: false

    readonly property bool lanMissing: card.status === "offline" && card.paired
    readonly property bool internetOffered: card.lanMissing && card.psnReachable
    readonly property bool signInHint: card.lanMissing && !card.psnReachable && !card.psnSignedIn

    signal playRequested
    signal playOverInternetRequested
    signal wakeRequested
    signal pairRequested
    signal refreshRequested
    signal cancelWakeRequested
    signal menuRequested(Item anchor)

    function triggerPrimary() {
        switch (card.status) {
        case "ready":
            card.playRequested();
            break;
        case "standby":
            card.wakeRequested();
            break;
        case "unpaired":
            card.pairRequested();
            break;
        case "offline":
            if (card.internetOffered) {
                card.playOverInternetRequested();
                break;
            }

            card.refreshRequested();
            break;
        }
    }

    function openMenu() {
        card.menuRequested(card.listMode ? listMore : gridMore);
    }

    implicitWidth: card.listMode ? 640 : 300
    implicitHeight: card.listMode ? listRow.implicitHeight : gridColumn.implicitHeight
    Accessible.role: Accessible.Grouping
    Accessible.name: card.name

    // The design's "More" button: a small dark tile with an ellipsis. The
    // list row gives it the action button's size.
    component MoreButton: T.Button {
        id: more

        property int boxWidth: 28
        property int boxHeight: 24
        property int boxRadius: 3
        property int glyphSize: 16

        implicitWidth: more.boxWidth
        implicitHeight: more.boxHeight
        focusPolicy: Qt.NoFocus
        hoverEnabled: true

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: "ellipsis"
                size: more.glyphSize
                color: more.hovered ? Theme.text : Theme.textMuted
            }
        }

        background: Rectangle {
            radius: more.boxRadius
            color: more.hovered ? Theme.surfaceHover : Theme.menu
        }
    }

    component ConsoleName: AppText {
        textStyle: "dialogTitle"
    }

    component SignInHint: AppText {
        textStyle: "detail"
        color: Theme.textMuted
        elide: Text.ElideRight
        text: "Entre na PSN em Rede para jogar pela internet"
    }

    component ConsoleAddress: AppText {
        textStyle: "detail"
        font.family: Theme.fontMono
        lineHeight: 16
        color: Theme.textMuted
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: Theme.surface
    }

    ColumnLayout {
        id: gridColumn

        anchors.fill: parent
        visible: !card.listMode
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space4
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            Layout.preferredHeight: 24

            StateBadge {
                status: card.status
            }

            Item {
                Layout.fillWidth: true
            }

            MoreButton {
                id: gridMore

                text: `Opções de ${card.name}`
                onClicked: card.openMenu()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Theme.space4
            Layout.leftMargin: Theme.space4
            Layout.rightMargin: Theme.space4
            Layout.bottomMargin: Theme.space4
            spacing: Theme.space3

            ConsoleName {
                Layout.fillWidth: true
                text: card.name
            }

            ConsoleAddress {
                Layout.fillWidth: true
                visible: !card.signInHint
                text: card.address
            }

            SignInHint {
                Layout.fillWidth: true
                visible: card.signInHint
            }

            CardAction {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.sizeControl
                overInternet: card.internetOffered
                status: card.status
                searching: card.searching
                consoleName: card.name
                onTriggered: card.triggerPrimary()
                onCancelRequested: card.cancelWakeRequested()
            }
        }
    }

    RowLayout {
        id: listRow

        anchors.fill: parent
        anchors.margins: Theme.space4
        visible: card.listMode
        spacing: Theme.space4

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space1

            Row {
                id: titleRow

                readonly property real nameWidth: Math.max(0, width - titleBadge.width - spacing)

                Layout.fillWidth: true
                spacing: Theme.space2

                ConsoleName {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, titleRow.nameWidth)
                    text: card.name
                }

                StateBadge {
                    id: titleBadge

                    anchors.verticalCenter: parent.verticalCenter
                    status: card.status
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3

                ConsoleAddress {
                    Layout.alignment: Qt.AlignVCenter
                    text: card.address
                }

                AppText {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    visible: !card.signInHint
                    text: card.activity
                    textStyle: "detail"
                    color: Theme.textSubtle
                }

                SignInHint {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    visible: card.signInHint
                }
            }
        }

        CardAction {
            Layout.preferredWidth: 220
            Layout.preferredHeight: Theme.sizeControlLg
            Layout.alignment: Qt.AlignVCenter
            overInternet: card.internetOffered
            status: card.status
            searching: card.searching
            consoleName: card.name
            onTriggered: card.triggerPrimary()
            onCancelRequested: card.cancelWakeRequested()
        }

        MoreButton {
            id: listMore

            boxWidth: Theme.sizeControlLg
            boxHeight: Theme.sizeControlLg
            boxRadius: Theme.radiusMd
            glyphSize: Theme.sizeIcon
            Layout.alignment: Qt.AlignVCenter
            text: `Opções de ${card.name}`
            onClicked: card.openMenu()
        }
    }

    FocusRing {
        targetRadius: Theme.radiusLg
        visible: card.focused
    }
}
