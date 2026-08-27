import QtQuick
import Distribuidora

// Selo de status de estoque: "ok" | "baixo" | "zerado".
// Sempre cor + ícone + texto (nunca só cor).
Rectangle {
    id: badge
    property string status: "ok"

    readonly property var _mapa: ({
        "ok":     { cor: Theme.success, texto: qsTr("OK"),     icone: "" },
        "baixo":  { cor: Theme.warning, texto: qsTr("Baixo"),  icone: "⚠ " },
        "zerado": { cor: Theme.danger,  texto: qsTr("Zerado"), icone: "✕ " }
    })
    readonly property var _info: _mapa[status] ? _mapa[status] : _mapa["ok"]

    implicitWidth: rotulo.implicitWidth + 18
    implicitHeight: 22
    radius: 6
    color: Qt.rgba(_info.cor.r, _info.cor.g, _info.cor.b, 0.15)

    Text {
        id: rotulo
        anchors.centerIn: parent
        text: badge._info.icone + badge._info.texto
        color: badge._info.cor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSm
        font.weight: Font.DemiBold
    }
}
