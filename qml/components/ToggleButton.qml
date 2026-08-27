import QtQuick
import QtQuick.Controls
import Distribuidora

// Botão de alternância (liga/desliga) com visual premium — substitui o CheckBox
// (sem "caixinha de marcar"). Desligado: botão contornado neutro. Ligado: pílula
// laranja com ✓. Expõe `checked` (herdado de AbstractButton), mantendo os
// bindings existentes que liam/escreviam .checked.
AbstractButton {
    id: control

    checkable: true
    hoverEnabled: true
    font.family: Theme.fontBase
    font.pixelSize: Theme.fontMd

    leftPadding: 16
    rightPadding: 18
    topPadding: 11
    bottomPadding: 11

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.checked ? Theme.primary
             : (control.down ? Theme.surfaceAlt : Theme.surface)
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.primary : Theme.borderStrong

        Behavior on color { ColorAnimation { duration: 130 } }
        Behavior on border.color { ColorAnimation { duration: 130 } }
    }

    contentItem: Row {
        spacing: 8
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.checked
            text: "✓"
            color: "#15100A"
            font.family: Theme.fontBase
            font.pixelSize: Theme.fontMd
            font.weight: Font.Bold
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.checked ? "#15100A" : Theme.text
            font.family: control.font.family
            font.pixelSize: control.font.pixelSize
            font.weight: Font.DemiBold
        }
    }
}
