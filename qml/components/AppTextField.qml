import QtQuick
import QtQuick.Controls
import Distribuidora

// Campo de texto estilizado na identidade (fundo de superfície, borda que fica
// laranja no foco). Substitui o TextField cru do Fusion.
TextField {
    id: control

    font.family: Theme.fontBase
    font.pixelSize: Theme.fontMd
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.primary
    selectedTextColor: "#FFFFFF"

    leftPadding: 12
    rightPadding: 12
    topPadding: 9
    bottomPadding: 9

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.primary
                     : (control.hovered ? Theme.ink : Theme.borderStrong)

        Behavior on border.color { ColorAnimation { duration: 120 } }
    }
}
