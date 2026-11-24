import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// The home screen: the consoles on the network and the saved ones.
FocusScope {
    id: page

    required property ConsolesController consoles
    required property SettingsController settings
    // The page's side margins, set by the shell for the window's width.
    property int sideMargin: Theme.space12
    property int currentIndex: 0

    readonly property bool hasConsoles: page.consoles.model.count > 0
    readonly property bool listMode: page.settings.consoleView === "list"
    // The design narrows the gaps along with the margins in a small window.
    readonly property int cardGap: page.sideMargin < Theme.space12 ? Theme.space3 : Theme.space4
    readonly property int minimumCardWidth: 220
    // The grid's card: the badge row over the body; the list's row: the
    // action and its padding.
    readonly property int cardHeight: page.listMode
                                      ? Theme.sizeControlLg + 2 * Theme.space4
                                      : 182
    // Room around the cards for the focus ring, which the scroller would clip.
    readonly property int ringRoom: Theme.sizeFocusRing + 1
    readonly property int topMargin: 26
    readonly property int emptyTopMargin: Theme.space10

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
                page.revealItem(cards.itemAt(i));
                return;
            }
        }
    }

    // The add tile follows the cards, so it is the last index.
    function navigableItem(index) {
        if (index === cards.count) {
            return addTile;
        }

        return cards.itemAt(index);
    }

    function moveCurrent(step) {
        const next = page.currentIndex + step;

        if (next < 0 || next > cards.count) {
            return;
        }

        page.currentIndex = next;
        page.revealItem(page.navigableItem(next));
    }

    function revealItem(item) {
        if (!item) {
            return;
        }

        const itemTop = item.mapToItem(content, 0, 0).y + content.y;
        const top = itemTop - page.ringRoom;
        const bottom = itemTop + item.height + page.ringRoom;

        if (top < scroller.contentY) {
            scroller.contentY = top;
            return;
        }

        if (bottom > scroller.contentY + scroller.height) {
            scroller.contentY = bottom - scroller.height;
        }
    }

    function triggerCurrent() {
        if (page.currentIndex === cards.count) {
            addDialog.openEmpty();
            return;
        }

        cards.itemAt(page.currentIndex)?.triggerPrimary();
    }

    function openCurrentMenu() {
        if (page.currentIndex === cards.count) {
            return;
        }

        cards.itemAt(page.currentIndex)?.openMenu();
    }

    function handleCardKey(event) {
        switch (event.key) {
        case Qt.Key_Left:
            page.moveCurrent(-1);
            break;
        case Qt.Key_Right:
            page.moveCurrent(1);
            break;
        case Qt.Key_Up:
            page.moveCurrent(-grid.columns);
            break;
        case Qt.Key_Down:
            page.moveCurrent(grid.columns);
            break;
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            page.triggerCurrent();
            break;
        case Qt.Key_Menu:
            page.openCurrentMenu();
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
            page.currentIndex = Math.min(page.currentIndex, page.consoles.model.count);
        }
    }

    ColumnLayout {
        id: emptyView

        anchors.fill: parent
        anchors.topMargin: page.emptyTopMargin
        anchors.leftMargin: page.sideMargin
        anchors.rightMargin: page.sideMargin
        visible: !page.hasConsoles
        spacing: Theme.space5

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Theme.space10
            visible: page.consoles.discoveryStatus === ConsolesController.Searching
            radius: Theme.radiusMd
            color: Theme.surface

            ScanIndicator {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        DiscoveryBanner {
            Layout.fillWidth: true
            status: page.consoles.discoveryStatus
            onRetryRequested: page.consoles.retryDiscovery()
        }

        EmptyState {
            id: emptyState

            Layout.fillWidth: true
            Layout.fillHeight: true
            onAddByAddressRequested: addDialog.openEmpty()
        }
    }

    Flickable {
        id: scroller

        anchors.fill: parent
        anchors.topMargin: page.topMargin - page.ringRoom
        anchors.leftMargin: page.sideMargin - page.ringRoom
        anchors.rightMargin: page.sideMargin - page.ringRoom
        visible: page.hasConsoles
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * page.ringRoom
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: content

            x: page.ringRoom
            y: page.ringRoom
            width: scroller.width - 2 * page.ringRoom
            spacing: 0

            DiscoveryBanner {
                Layout.fillWidth: true
                Layout.bottomMargin: 22
                status: page.consoles.discoveryStatus
                onRetryRequested: page.consoles.retryDiscovery()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: page.cardGap

                StatusCount {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    status: "ready"
                    label: "Pronto"
                    count: page.consoles.model.readyCount
                }

                StatusCount {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    status: "standby"
                    label: "Em repouso"
                    count: page.consoles.model.standbyCount
                }

                StatusCount {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 0
                    status: "unpaired"
                    label: "Não pareado"
                    count: page.consoles.model.unpairedCount
                }

                // The design gives the toggle a whole cell beside the counters
                // in a wide window, and only its own width in a small one.
                Item {
                    Layout.fillWidth: page.sideMargin >= Theme.space12
                    Layout.preferredWidth: page.sideMargin >= Theme.space12
                                                ? -1
                                                : viewToggle.implicitWidth
                    Layout.preferredHeight: Theme.space10

                    ViewToggle {
                        id: viewToggle

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        mode: page.settings.consoleView
                        onActivated: mode => page.settings.consoleView = mode
                    }
                }
            }

            AppText {
                Layout.topMargin: 22
                Layout.bottomMargin: Theme.space4
                Layout.leftMargin: 2
                text: "Lista de consoles"
                textStyle: "overline"
                color: Theme.textSubtle
                Accessible.role: Accessible.Heading
            }

            FocusScope {
                id: cardsArea

                readonly property real cardWidth:
                    (width - page.cardGap * (grid.columns - 1)) / grid.columns

                Layout.fillWidth: true
                implicitHeight: grid.implicitHeight
                activeFocusOnTab: true
                Accessible.role: Accessible.List
                Accessible.name: "Consoles"
                Keys.onPressed: event => page.handleCardKey(event)

                Grid {
                    id: grid

                    columns: page.listMode ? 1 : Math.max(1, Math.floor(
                        (cardsArea.width + page.cardGap) / (page.minimumCardWidth + page.cardGap)))
                    columnSpacing: page.cardGap
                    rowSpacing: page.cardGap

                    Repeater {
                        id: cards

                        model: page.consoles.model

                        delegate: ConsoleCard {
                            id: consoleCard

                            width: cardsArea.cardWidth
                            height: page.cardHeight
                            listMode: page.listMode
                            focused: cardsArea.activeFocus && consoleCard.index === page.currentIndex
                            onPlayRequested: page.consoles.play(consoleCard.key)
                            onWakeRequested: page.consoles.wake(consoleCard.key)
                            onCancelWakeRequested: page.consoles.cancelWake(consoleCard.key)
                            onPairRequested: pairingDialog.openFor(consoleCard.key, consoleCard.name)
                            onRefreshRequested: page.consoles.refresh(consoleCard.key)
                            onMenuRequested: anchor => cardMenu.openFor(consoleCard, anchor)
                        }
                    }

                    AddConsoleTile {
                        id: addTile

                        width: cardsArea.cardWidth
                        height: page.cardHeight
                        listMode: page.listMode
                        focused: cardsArea.activeFocus && page.currentIndex === cards.count
                        onClicked: addDialog.openEmpty()
                    }
                }
            }
        }
    }

    PopupMenu {
        id: cardMenu

        property string consoleKey
        property string consoleName
        property bool consoleSaved
        property bool consolePaired

        function openFor(card, anchor) {
            cardMenu.consoleKey = card.key;
            cardMenu.consoleName = card.name;
            cardMenu.consoleSaved = card.saved;
            cardMenu.consolePaired = card.paired;
            cardMenu.parent = anchor;
            cardMenu.x = anchor.width - cardMenu.width;
            cardMenu.y = anchor.height + Theme.space1;
            cardMenu.open();
        }

        MenuRow {
            text: "Parear"
            iconName: "link-2"
            enabled: !cardMenu.consolePaired
            onTriggered: pairingDialog.openFor(cardMenu.consoleKey, cardMenu.consoleName)
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
