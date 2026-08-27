import QtQuick
import QtQuick.Controls
import Distribuidora

// SpinBox estilizado (botões − / + nas laterais, editável). Substitui o SpinBox
// cru do Fusion.
SpinBox {
    id: control

    editable: true
    font.family: Theme.fontBase
    font.pixelSize: Theme.fontMd

    leftPadding: 30
    rightPadding: 30
    topPadding: 8
    bottomPadding: 8

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.primary
                     : (control.hovered ? Theme.ink : Theme.borderStrong)
    }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: Theme.text
        selectionColor: Theme.primary
        selectedTextColor: "#FFFFFF"
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    down.indicator: Rectangle {
        x: 0
        height: parent.height
        width: 30
        radius: Theme.radiusSm
        color: control.down.pressed ? Theme.surfaceAlt : "transparent"
        Text {
            text: "−"   // −
            anchors.centerIn: parent
            color: control.enabled ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontLg
        }
    }

    up.indicator: Rectangle {
        x: control.width - width
        height: parent.height
        width: 30
        radius: Theme.radiusSm
        color: control.up.pressed ? Theme.surfaceAlt : "transparent"
        Text {
            text: "+"
            anchors.centerIn: parent
            color: control.enabled ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontLg
        }
    }
}
