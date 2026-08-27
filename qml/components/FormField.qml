import QtQuick
import QtQuick.Layouts
import Distribuidora

// Campo de formulário: rótulo em cima + o controle (colocado como filho).
// É um ColumnLayout (e não um Column) para esticar corretamente a largura em
// qualquer contexto — inclusive dois lado a lado dentro de um RowLayout, onde um
// Column colapsava para a largura do rótulo e cortava o texto digitado.
//
// Uso:
//   FormField { label: "Nome"; Layout.fillWidth: true
//       AppTextField { width: parent.width }   // parent = o slot, que preenche
//   }
ColumnLayout {
    id: root
    property string label: ""
    default property alias content: slot.data
    spacing: 5

    Text {
        text: root.label
        visible: root.label !== ""
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Font.DemiBold
        Layout.fillWidth: true
    }

    // O controle vai aqui dentro (Item comum, não gerido por layout), então pode
    // usar `width: parent.width` livremente. O slot preenche a largura do campo.
    Item {
        id: slot
        Layout.fillWidth: true
        implicitHeight: childrenRect.height
    }
}
