import QtQuick
import QtQuick.Controls
import Distribuidora

// ComboBox estilizado na identidade (fundo, borda de foco, chevron, popup e
// itens no tema). Substitui o ComboBox cru do Fusion.
ComboBox {
    id: control

    font.family: Theme.fontBase
    font.pixelSize: Theme.fontMd

    leftPadding: 12
    rightPadding: 36
    topPadding: 9
    bottomPadding: 9

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.primary
                     : (control.hovered ? Theme.ink : Theme.borderStrong)
    }

    contentItem: Text {
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: AppIcon {
        name: "chevron"
        size: 16
        color: Theme.textMuted
        x: control.width - width - 12
        y: control.topPadding + (control.availableHeight - height) / 2
    }

    delegate: ItemDelegate {
        id: cbDelegate
        width: ListView.view ? ListView.view.width : control.width
        required property int index
        required property var modelData
        padding: 8

        contentItem: Text {
            text: (control.textRole && cbDelegate.modelData
                   && cbDelegate.modelData[control.textRole] !== undefined)
                  ? cbDelegate.modelData[control.textRole]
                  : cbDelegate.modelData
            color: Theme.text
            font: control.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        highlighted: control.highlightedIndex === index
        background: Rectangle {
            radius: 6
            color: cbDelegate.highlighted ? Theme.accentSoft : "transparent"
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(listaPopup.contentHeight + 8, 300)

        background: Rectangle {
            radius: Theme.radiusSm
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
        }
        contentItem: ListView {
            id: listaPopup
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
