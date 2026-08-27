import QtQuick
import QtQuick.Controls
import Distribuidora

// Botão nas cores da marca (o Fusion padrão sairia azul).
//   kind: "accent" (laranja) | "ink" (preto) | "default" (contorno) | "ghost"
Button {
    id: control
    property string kind: "default"

    font.family: Theme.fontBase
    font.pixelSize: Theme.fontMd
    font.weight: Font.DemiBold

    topPadding: 9
    bottomPadding: 9
    leftPadding: 16
    rightPadding: 16

    readonly property bool _filled: kind === "accent" || kind === "ink"
    readonly property color _base: kind === "accent" ? Theme.primary
                                 : kind === "ink" ? Theme.ink
                                 : kind === "ghost" ? "transparent"
                                 : Theme.surface
    readonly property color _hover: kind === "accent" ? Theme.primaryHover
                                  : kind === "ink" ? Theme.inkStrong
                                  : Theme.surfaceAlt
    readonly property color _fg: _filled ? "#FFFFFF"
                               : kind === "ghost" ? Theme.textMuted
                               : Theme.text

    background: Rectangle {
        radius: Theme.radiusSm
        color: (control.down || control.hovered) ? control._hover : control._base
        border.width: control.kind === "default" ? 1 : 0
        border.color: control.hovered ? Theme.ink : Theme.borderStrong
    }

    contentItem: Text {
        text: control.text
        color: control._fg
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
