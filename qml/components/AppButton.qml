import QtQuick
import QtQuick.Controls
import Distribuidora

// Botão nas cores da marca (o Fusion padrão sairia azul).
//   kind: "accent" (laranja) | "ink" (preto) | "default" (contorno) | "ghost"
//       | "perigo" (contorno vermelho, para ações que desfazem/apagam)
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
    readonly property bool _perigo: kind === "perigo"
    readonly property color _base: kind === "accent" ? Theme.primary
                                 : kind === "ink" ? Theme.ink
                                 : kind === "ghost" ? "transparent"
                                 : kind === "perigo" ? "transparent"
                                 : Theme.surface
    readonly property color _hover: kind === "accent" ? Theme.primaryHover
                                  : kind === "ink" ? Theme.inkStrong
                                  : Theme.surfaceAlt
    readonly property color _fg: _filled ? "#FFFFFF"
                               : _perigo ? Theme.danger
                               : kind === "ghost" ? Theme.textMuted
                               : Theme.text

    background: Rectangle {
        radius: Theme.radiusSm
        color: (control.down || control.hovered) ? control._hover : control._base
        border.width: (control.kind === "default" || control._perigo) ? 1 : 0
        border.color: control._perigo ? Theme.danger
                    : (control.hovered ? Theme.ink : Theme.borderStrong)
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
