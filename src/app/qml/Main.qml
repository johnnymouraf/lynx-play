import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property AppController controller

    width: 960
    height: 600
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "Lynx Play"
    color: "#0E1116"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Lynx Play"
            color: "#F2F4F8"
            font.pixelSize: 40
            font.weight: Font.DemiBold
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Remote Play para PS5 com recursos RTX"
            color: "#9AA4B2"
            font.pixelSize: 16
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 24
            text: window.controller.remotePlayReady
                  ? "Biblioteca de Remote Play pronta"
                  : "Não foi possível iniciar o Remote Play. Veja o log."
            color: window.controller.remotePlayReady ? "#4ADE80" : "#F87171"
            font.pixelSize: 14
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Versão " + window.controller.version
            color: "#5B6573"
            font.pixelSize: 12
        }
    }
}
