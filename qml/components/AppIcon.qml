import QtQuick
import QtQuick.Shapes

// Ícone de linha monocromático, colorível pelo tema (substitui emojis).
// Os traços vêm em coordenadas 0..24 (como o SVG do mockup) e são escalados
// para `size`. Círculos são desenhados como dois semi-arcos.
Item {
    id: root
    property string name: ""
    property color color: "#000000"
    property real size: 20
    property real strokeWidth: 1.6

    implicitWidth: size
    implicitHeight: size

    readonly property var _paths: ({
        "dashboard": "M3.5 3.5h7v7h-7z M13.5 3.5h7v7h-7z M3.5 13.5h7v7h-7z M13.5 13.5h7v7h-7z",
        "pdv": "M3 4h2.2l2 11a1.6 1.6 0 0 0 1.6 1.3h7.6a1.6 1.6 0 0 0 1.6-1.2L21 8H6.2 M8.1 20a1.4 1.4 0 1 0 2.8 0a1.4 1.4 0 1 0 -2.8 0 M16.1 20a1.4 1.4 0 1 0 2.8 0a1.4 1.4 0 1 0 -2.8 0",
        "produtos": "M12 3 4 7.2v9.6L12 21l8-4.2V7.2Z M4 7.2 12 11l8-3.8 M12 21V11",
        "estoque": "M12 3 3 7.5l9 4.5 9-4.5Z M3 12.5 12 17l9-4.5 M3 16.5 12 21l9-4.5",
        "compras": "M3 6.5h11v9.5H3z M14 10h3.5l3 3v3H14z M5.4 18.5a1.6 1.6 0 1 0 3.2 0a1.6 1.6 0 1 0 -3.2 0 M15.9 18.5a1.6 1.6 0 1 0 3.2 0a1.6 1.6 0 1 0 -3.2 0",
        "clientes": "M5.8 8a3.2 3.2 0 1 0 6.4 0a3.2 3.2 0 1 0 -6.4 0 M3.6 19a5.4 5.4 0 0 1 10.8 0 M15.2 5.6a2.8 2.8 0 0 1 0 5.4 M17.4 19a5 5 0 0 0 -2-3.8",
        "financeiro": "M3.5 6h17v12.5h-17z M3.5 10h17 M15.2 14a1.3 1.3 0 1 0 2.6 0a1.3 1.3 0 1 0 -2.6 0",
        "relatorios": "M5 20V11 M11.5 20V4.5 M18 20v-6.5",
        "usuarios": "M4.3 8.5a4.2 4.2 0 1 0 8.4 0a4.2 4.2 0 1 0 -8.4 0 M11.6 11.6 20 20 M16.5 16.5l2.2-2.2",
        "buscar": "M4 10.5a6.5 6.5 0 1 0 13 0a6.5 6.5 0 1 0 -13 0 M15.8 15.8 20 20",
        "fechar": "M6 6 18 18 M18 6 6 18",
        "chevron": "M6 10 12 16 18 10",
        "mais": "M12 5v14 M5 12h14",
        "codigo": "M4 6v12 M7 6v12 M9.5 6v12 M13 6v12 M16 6v12 M18 6v12 M20.5 6v12",
        "vendas": "M4.5 5.5h15v13h-15z M4.5 9.5h15 M8 13h8 M8 16h5",
        "caixa": "M3.5 10.5h17v9h-17z M3.5 14.2h17 M10.2 16.9h3.6 M8.6 7a3.4 3.4 0 1 0 6.8 0a3.4 3.4 0 1 0 -6.8 0",
        "vencimento": "M4 6.5h16v13.5H4z M4 10.5h16 M8 4.2v4 M16 4.2v4 M12 13v3.2 M12 18.4v.6",
        "backup": "M12 3 5 6v5.5c0 4 3 7 7 8.2 4-1.2 7-4.2 7-8.2V6Z M9 11.8l2.2 2.2 4.3-4.3",
        "relmobile": "M7.5 2.5h9a1.5 1.5 0 0 1 1.5 1.5v16a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 6 20V4a1.5 1.5 0 0 1 1.5-1.5Z M10 19h4"
    })

    readonly property real _sw: name === "relatorios" ? 2.0 : strokeWidth

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: root.size / 24
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            fillColor: "transparent"
            strokeWidth: root._sw
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root._paths[root.name] !== undefined ? root._paths[root.name] : "" }
        }
    }
}
