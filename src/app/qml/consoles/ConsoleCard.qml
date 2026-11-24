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
    required property bool searching
    property bool listMode: false
    property bool focused: false

    signal playRequested
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

    // The design's "More" button: a small dark tile with an ellipsis.
    component MoreButton: T.Button {
        id: more

        implicitWidth: 28
        implicitHeight: 24
        focusPolicy: Qt.NoFocus
        hoverEnabled: true

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: "ellipsis"
                size: 16
                color: more.hovered ? Theme.text : Theme.textMuted
            }
        }

        background: Rectangle {
            radius: 3
            color: more.hovered ? Theme.surfaceHover : Theme.menu
        }
    }

    component ConsoleName: AppText {
        textStyle: "dialogTitle"
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
                text: card.address
            }

            CardAction {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.sizeControl
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

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        implicitHeight: Theme.sizeControlLg + 2 * Theme.space4
        visible: card.listMode
        spacing: Theme.space4

        Item {
            Layout.preferredWidth: 120
            Layout.preferredHeight: badge.implicitHeight

            StateBadge {
                id: badge

                status: card.status
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            ConsoleName {
                Layout.fillWidth: true
                text: card.name
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space3

                ConsoleAddress {
                    text: card.address
                }

                AppText {
                    Layout.fillWidth: true
                    text: card.activity
                    textStyle: "detail"
                    color: Theme.textSubtle
                }
            }
        }

        CardAction {
            Layout.preferredWidth: 220
            Layout.preferredHeight: Theme.sizeControlLg
            status: card.status
            searching: card.searching
            consoleName: card.name
            onTriggered: card.triggerPrimary()
            onCancelRequested: card.cancelWakeRequested()
        }

        MoreButton {
            id: listMore

            text: `Opções de ${card.name}`
            onClicked: card.openMenu()
        }
    }

    FocusRing {
        targetRadius: Theme.radiusLg
        visible: card.focused
    }
}
