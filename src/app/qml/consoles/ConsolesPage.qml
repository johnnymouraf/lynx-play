import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// The home screen: the consoles on the network and the saved ones.
FocusScope {
    id: page

    required property ConsolesController consoles

    readonly property bool hasConsoles: page.consoles.model.count > 0
    readonly property int minimumCardWidth: 280
    property int currentIndex: 0

    signal toastRequested(string tone, string title, string message)

    function focusFirstControl() {
        if (page.hasConsoles) {
            cardsArea.forceActiveFocus(Qt.TabFocusReason);
            return;
        }

        emptyState.focusAction();
    }

    function focusCard(key) {
        for (let i = 0; i < cards.count; ++i) {
            if (cards.itemAt(i).key === key) {
                page.currentIndex = i;
                cardsArea.forceActiveFocus(Qt.TabFocusReason);
                page.revealCard(cards.itemAt(i));
                return;
            }
        }
    }

    function moveCurrent(step) {
        const next = page.currentIndex + step;

        if (next < 0 || next >= cards.count) {
            return;
        }

        page.currentIndex = next;
        page.revealCard(cards.itemAt(next));
    }

    function revealCard(card) {
        if (!card) {
            return;
        }

        const cardTop = card.mapToItem(content, 0, 0).y + content.y;
        const top = cardTop - Theme.space2;
        const bottom = cardTop + card.height + Theme.space2;

        if (top < scroller.contentY) {
            scroller.contentY = top;
            return;
        }

        if (bottom > scroller.contentY + scroller.height) {
            scroller.contentY = bottom - scroller.height;
        }
    }

    function handleCardKey(event) {
        const current = cards.itemAt(page.currentIndex);

        switch (event.key) {
        case Qt.Key_Left:
            page.moveCurrent(-1);
            break;
        case Qt.Key_Right:
            page.moveCurrent(1);
            break;
        case Qt.Key_Up:
            page.moveCurrent(-cardsArea.columns);
            break;
        case Qt.Key_Down:
            page.moveCurrent(cardsArea.columns);
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            current?.triggerPrimary();
            break;
        case Qt.Key_Menu:
            current?.openMenu();
            break;
        default:
            return;
        }

        event.accepted = true;
    }

    onHasConsolesChanged: {
        if (page.activeFocus) {
            page.focusFirstControl();
        }
    }

    Connections {
        target: page.consoles.model

        function onCountChanged() {
            page.currentIndex = Math.min(page.currentIndex, Math.max(0, cards.count - 1));
        }
    }

    Flickable {
        id: scroller

        anchors.fill: parent
        anchors.topMargin: Theme.space6
        anchors.leftMargin: Theme.space8
        anchors.rightMargin: Theme.space8
        anchors.bottomMargin: Theme.space6
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * Theme.space2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: content

            x: Theme.space2
            y: Theme.space2
            width: scroller.width - 2 * Theme.space2
            spacing: Theme.space8

            RowLayout {
                id: header

                Layout.fillWidth: true
                spacing: Theme.space6

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    spacing: Theme.space1

                    AppText {
                        text: "Consoles"
                        textStyle: "titleLarge"
                        Accessible.role: Accessible.Heading
                    }

                    ScanIndicator {
                        visible: page.consoles.discoveryStatus === ConsolesController.Searching
                    }
                }

                ActionButton {
                    Layout.alignment: Qt.AlignBottom
                    visible: page.hasConsoles
                    iconName: "plus"
                    text: "Adicionar pelo IP"
                    onClicked: addDialog.openEmpty()
                }
            }

            DiscoveryBanner {
                Layout.fillWidth: true
                status: page.consoles.discoveryStatus
                onRetryRequested: page.consoles.retryDiscovery()
            }

            FocusScope {
                id: cardsArea

                readonly property int columns: Math.max(1, Math.floor(
                    (width + Theme.space6) / (page.minimumCardWidth + Theme.space6)))
                readonly property real cardWidth: (width - Theme.space6 * (columns - 1)) / columns

                Layout.fillWidth: true
                implicitHeight: grid.implicitHeight
                visible: page.hasConsoles
                activeFocusOnTab: true
                Accessible.role: Accessible.List
                Accessible.name: "Consoles"
                Keys.onPressed: event => page.handleCardKey(event)

                Grid {
                    id: grid

                    columns: cardsArea.columns
                    columnSpacing: Theme.space6
                    rowSpacing: Theme.space6

                    Repeater {
                        id: cards

                        model: page.consoles.model

                        delegate: ConsoleCard {
                            id: consoleCard

                            width: cardsArea.cardWidth
                            focused: cardsArea.activeFocus && consoleCard.index === page.currentIndex
                            onPlayRequested: page.consoles.play(consoleCard.key)
                            onPairRequested: pairingDialog.openFor(consoleCard.key, consoleCard.name)
                            onRefreshRequested: page.consoles.refresh(consoleCard.key)
                            onMenuRequested: anchor => cardMenu.openFor(consoleCard, anchor)
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(emptyState.implicitHeight,
                                                 scroller.height - 2 * Theme.space2
                                                 - header.height - content.spacing)
                visible: !page.hasConsoles

                EmptyState {
                    id: emptyState

                    anchors.centerIn: parent
                    width: parent.width
                    onAddByAddressRequested: addDialog.openEmpty()
                }
            }
        }
    }

    PopupMenu {
        id: cardMenu

        property string consoleKey
        property string consoleName
        property bool consoleSaved

        function openFor(card, anchor) {
            cardMenu.consoleKey = card.key;
            cardMenu.consoleName = card.name;
            cardMenu.consoleSaved = card.saved;
            cardMenu.parent = anchor;
            cardMenu.x = anchor.width - cardMenu.width;
            cardMenu.y = anchor.height + Theme.space1;
            cardMenu.open();
        }

        MenuRow {
            text: "Renomear"
            iconName: "pencil"
            onTriggered: renameDialog.openFor(cardMenu.consoleKey, cardMenu.consoleName)
        }

        MenuRow {
            text: "Esquecer"
            iconName: "trash"
            danger: true
            enabled: cardMenu.consoleSaved
            onTriggered: forgetDialog.openFor(cardMenu.consoleKey, cardMenu.consoleName)
        }
    }

    RenameDialog {
        id: renameDialog

        onRenameRequested: (key, name) => page.consoles.rename(key, name)
    }

    ForgetDialog {
        id: forgetDialog

        onForgetRequested: key => page.consoles.forget(key)
    }

    AddByAddressDialog {
        id: addDialog

        consoles: page.consoles
        onAdded: key => page.focusCard(key)
    }

    PairingDialog {
        id: pairingDialog

        consoles: page.consoles
        onPaired: (key, name) => page.toastRequested("ready", `${name} pareado`,
                                                      "Ele já aparece na lista de consoles.")
    }
}
