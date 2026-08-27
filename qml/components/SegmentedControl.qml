import QtQuick
import Distribuidora

// Alternador segmentado (pílula) com destaque deslizante — substitui o
// TabBar/TabButton cru por um toggle premium na identidade da marca.
// Uso: SegmentedControl { options: ["Entrada","Inventário"]; onCurrentIndexChanged: ... }
Item {
    id: root

    property var options: []
    property int currentIndex: 0
    readonly property int _n: Math.max(1, options.length)
    readonly property real _seg: width / _n

    implicitHeight: 40
    implicitWidth: 260

    // Trilho
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surfaceAlt
        border.color: Theme.border
        border.width: 1
    }

    // Destaque deslizante (segmento ativo)
    Rectangle {
        id: thumb
        y: 3
        height: parent.height - 6
        width: root._seg - 6
        x: root.currentIndex * root._seg + 3
        radius: height / 2
        color: Theme.primary

        Behavior on x { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
    }

    Row {
        anchors.fill: parent
        Repeater {
            model: root.options
            delegate: Item {
                id: seg
                required property int index
                required property var modelData
                width: root._seg
                height: root.height

                readonly property bool ativo: seg.index === root.currentIndex

                Text {
                    anchors.centerIn: parent
                    text: seg.modelData
                    color: seg.ativo ? "#15100A"
                         : (mouse.containsMouse ? Theme.text : Theme.textMuted)
                    font.family: Theme.fontBase
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.currentIndex = seg.index
                }
            }
        }
    }
}
