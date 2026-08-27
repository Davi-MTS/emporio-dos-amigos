import QtQuick
import Distribuidora

// Tela genérica "em construção", usada enquanto os módulos são implementados.
Rectangle {
    id: tela
    property string titulo: qsTr("Em construção")

    color: Theme.background

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingSm

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: tela.titulo
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.fontXxl
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Módulo ainda não implementado.")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMd
        }
    }
}
