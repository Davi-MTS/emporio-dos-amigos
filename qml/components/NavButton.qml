import QtQuick
import QtQuick.Controls
import Distribuidora

// Item de navegação da barra lateral (ícone de linha + rótulo).
AbstractButton {
    id: control

    property bool ativo: false
    property string icone: ""   // nome do ícone (ver AppIcon)

    implicitHeight: 42
    padding: Theme.spacingMd
    hoverEnabled: true

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.ativo
                   ? Theme.sidebarActive
                   : (control.hovered ? Qt.rgba(1, 1, 1, 0.05) : "transparent")

        // Barra de acento à esquerda quando ativo.
        Rectangle {
            visible: control.ativo
            width: 3
            radius: 2
            color: Theme.primary
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8
            anchors.bottomMargin: 8
        }
    }

    contentItem: Row {
        spacing: Theme.spacingMd

        AppIcon {
            name: control.icone
            size: 20
            color: control.ativo ? Theme.textOnDark : Theme.textOnDarkMuted
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            text: control.text
            font.family: Theme.fontBase
            font.pixelSize: Theme.fontMd
            font.weight: control.ativo ? Font.DemiBold : Font.Normal
            anchors.verticalCenter: parent.verticalCenter
            color: control.ativo ? Theme.textOnDark : Theme.textOnDarkMuted
        }
    }
}
