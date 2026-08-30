import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Vencimento — o que está perto de virar prejuízo.
//
// Doce, salgadinho e gelo vencem, e duas cargas do mesmo produto chegam com
// validades diferentes. Por isso o controle é por REMESSA (lote), e não uma
// data no cadastro do produto: a data única seria sobrescrita pela carga nova e
// a mercadoria velha sumiria do radar — justo a que precisa sair primeiro.
Rectangle {
    id: tela
    color: Theme.background

    property var resumo: ({})
    property var divergencias: []
    property int filtroDias: 30

    ListModel { id: lotesModel }

    Component.onCompleted: carregar()
    function carregar() {
        resumo = App.resumoVencimento();
        divergencias = App.divergenciasDeLote();
        lotesModel.clear();
        var l = App.lotes(filtroDias);
        for (var i = 0; i < l.length; i++)
            lotesModel.append(l[i]);
    }

    function fmtData(iso) {
        if (!iso || iso.length === 0) return "—";
        var d = new Date(iso);
        return isNaN(d.getTime()) ? iso : Qt.formatDate(d, "dd/MM/yyyy");
    }

    // Texto e cor por urgência: é o que o dono lê antes de decidir promoção.
    function textoPrazo(dias) {
        if (dias < 0)  return qsTr("venceu há ") + (-dias) + qsTr(" dia(s)");
        if (dias === 0) return qsTr("vence HOJE");
        if (dias === 1) return qsTr("vence amanhã");
        return qsTr("vence em ") + dias + qsTr(" dias");
    }
    function corPrazo(dias) {
        if (dias < 0)  return Theme.danger;
        if (dias <= 7) return Theme.warning;
        return Theme.textMuted;
    }

    component Cartao: Rectangle {
        property string rotulo: ""
        property string valor: ""
        property string nota: ""
        property color cor: Theme.text
        width: 200
        height: 78
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            spacing: 1
            Text {
                text: rotulo
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
            Text {
                text: valor
                color: cor
                font.family: Theme.fontBase
                font.pixelSize: Theme.fontXl
                font.weight: Font.Bold
            }
            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: nota
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Cartao {
                rotulo: qsTr("Vencidos")
                valor: "" + (tela.resumo.vencidos || 0)
                nota: (tela.resumo.quantidadeVencida || 0) + qsTr(" unidades paradas")
                cor: (tela.resumo.vencidos || 0) > 0 ? Theme.danger : Theme.success
            }
            Cartao {
                rotulo: qsTr("Vence em 7 dias")
                valor: "" + (tela.resumo.venceEm7 || 0)
                nota: qsTr("hora de girar")
                cor: (tela.resumo.venceEm7 || 0) > 0 ? Theme.warning : Theme.text
            }
            Cartao {
                rotulo: qsTr("Vence em 30 dias")
                valor: "" + (tela.resumo.venceEm30 || 0)
                nota: qsTr("de olho")
                cor: Theme.text
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            SegmentedControl {
                id: filtro
                width: 330
                height: 42
                options: [qsTr("Vencidos"), qsTr("7 dias"), qsTr("30 dias"), qsTr("Tudo")]
                currentIndex: 2
                onCurrentIndexChanged: {
                    tela.filtroDias = (currentIndex === 0) ? 0
                                    : (currentIndex === 1) ? 7
                                    : (currentIndex === 2) ? 30 : -1;
                    tela.carregar();
                }
            }
            AppButton { kind: "ghost"; text: qsTr("↻ Atualizar"); onClicked: tela.carregar() }
        }

        // Sem esta explicação, alguém acharia que o sistema perdeu mercadoria.
        Rectangle {
            Layout.fillWidth: true
            visible: tela.divergencias.length > 0
            radius: Theme.radiusSm
            color: Theme.surface
            border.color: Theme.warning
            implicitHeight: divCol.implicitHeight + 2 * Theme.spacingMd
            ColumnLayout {
                id: divCol
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Estoque e lotes não batem em ") + tela.divergencias.length
                          + qsTr(" produto(s). Normal quando parte da mercadoria entrou sem validade informada, ou depois de um ajuste de inventário — o ajuste mexe no saldo, mas não sabe de qual remessa tirar. O estoque continua correto; só a validade dessa parte não é acompanhada.")
                    color: Theme.warning
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                }
                Repeater {
                    model: tela.divergencias
                    delegate: Text {
                        required property var modelData
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: "· " + modelData.produto + ": "
                              + (modelData.diferenca > 0
                                 ? qsTr("+") + modelData.diferenca + qsTr(" sem validade")
                                 : "" + modelData.diferenca)
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontXs
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 38
                    color: Theme.surfaceAlt
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingSm
                        Text { text: qsTr("Produto"); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { visible: listaLotes.mostrarLote; text: qsTr("Lote"); Layout.preferredWidth: 90; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Qtd"); Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Validade"); Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                }

                ListView {
                    id: listaLotes
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: lotesModel
                    ScrollBar.vertical: ScrollBar {}

                    // Coluna some antes de espremer o nome do produto.
                    readonly property bool mostrarLote: width > 520

                    delegate: Rectangle {
                        id: linha
                        required property string produto
                        required property string unidade
                        required property string codigo
                        required property string validade
                        required property var quantidade
                        required property int dias
                        width: ListView.view.width
                        height: 50
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            spacing: Theme.spacingSm

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 0
                                Text {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: linha.produto
                                    color: Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: tela.textoPrazo(linha.dias)
                                    color: tela.corPrazo(linha.dias)
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                }
                            }
                            Text {
                                visible: listaLotes.mostrarLote
                                Layout.preferredWidth: 90
                                elide: Text.ElideRight
                                text: linha.codigo.length > 0 ? linha.codigo : "—"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSm
                            }
                            Text {
                                Layout.preferredWidth: 80
                                horizontalAlignment: Text.AlignRight
                                text: linha.quantidade + " " + linha.unidade
                                color: Theme.text
                                font.pixelSize: Theme.fontMd
                            }
                            Text {
                                Layout.preferredWidth: 100
                                horizontalAlignment: Text.AlignRight
                                text: tela.fmtData(linha.validade)
                                color: tela.corPrazo(linha.dias)
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 2 * Theme.spacingLg
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        visible: listaLotes.count === 0
                        text: qsTr("Nada vencendo neste período.\n\nA validade é informada na entrada de mercadoria (Estoque → Entrada). Produtos que não vencem em prazo curto podem ficar sem ela.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                    }
                }
            }
        }
    }
}
