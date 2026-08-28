import QtQuick
import QtQuick.Controls
import Distribuidora

// Diálogo padronizado na identidade: fundo de superfície com borda arredondada,
// título em Fraunces com divisória, e fundo escurecido atrás (em vez do visual
// cru do Fusion). Use no lugar de Dialog.
Dialog {
    id: control
    modal: true
    padding: Theme.spacingLg

    // Nunca ultrapassar a janela: um diálogo mais alto que a tela sai pelas
    // bordas e os botões de confirmar ficam inalcançáveis. Diálogos com muito
    // conteúdo usam ScrollView por dentro para o excedente continuar acessível.
    readonly property real alturaMaxima: (parent ? parent.height : 900) - 2 * Theme.spacingLg
    height: Math.min(implicitHeight, alturaMaxima)

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
    }

    header: Item {
        implicitHeight: control.title.length > 0 ? 56 : 0
        visible: control.title.length > 0
        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            verticalAlignment: Text.AlignVCenter
            text: control.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.fontXl
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }
}
