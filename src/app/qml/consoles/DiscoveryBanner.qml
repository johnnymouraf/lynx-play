import QtQuick
import LynxPlay

// Why the network search isn't running, with a way to start it again.
Banner {
    id: banner

    property int status: ConsolesController.Searching

    signal retryRequested

    visible: false
    tone: "danger"
    onActionTriggered: banner.retryRequested()

    states: [
        State {
            name: "noNetwork"
            when: banner.status === ConsolesController.NoNetwork

            PropertyChanges {
                target: banner
                visible: true
                title: "Sem conexão de rede"
                message: "Conecte o PC à mesma rede do PS5 e tente de novo."
                actionText: "Tentar de novo"
            }
        },
        State {
            name: "failed"
            when: banner.status === ConsolesController.Failed

            PropertyChanges {
                target: banner
                visible: true
                title: "Não foi possível procurar consoles"
                message: "A busca na rede não começou. Veja o log em %LOCALAPPDATA%\LynxPlay\logs."
                actionText: "Tentar de novo"
            }
        },
        State {
            name: "unavailable"
            when: banner.status === ConsolesController.Unavailable

            PropertyChanges {
                target: banner
                visible: true
                title: "O Remote Play não iniciou"
                message: "Feche e abra o Lynx Play de novo. Se continuar, veja o log em %LOCALAPPDATA%\LynxPlay\logs."
            }
        }
    ]
}
