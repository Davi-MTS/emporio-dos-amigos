import QtQuick
import QtQuick.Layouts
import Distribuidora

// Dashboard com dados reais (src/domain/relatorios/).
Rectangle {
    id: tela
    color: Theme.background

    readonly property bool veFinanceiro: {
        var p = (App.usuarioAtual && App.usuarioAtual.permissoes) ? App.usuarioAtual.permissoes : ({});
        return p.tudo === true || p.ve_financeiro === true;
    }

    property var kpis: ({})
    property var maisVendidos: []
    property var fin: ({})

    Component.onCompleted: carregar()
    function carregar() {
        kpis = App.dashboard();
        maisVendidos = App.relatorioMaisVendidos(7, 5);
        fin = App.resumoFinanceiro();
    }

    // Pedido de navegação para quem hospeda a tela (Main.qml).
    signal navegar(string rota)

    component KpiCard: Rectangle {
        property string rotulo: ""
        property string valor: ""
        property string nota: ""
        property color corValor: Theme.text
        // Quando `rota` está preenchida, o cartão vira um atalho clicável.
        property string rota: ""
        Layout.fillWidth: true
        Layout.preferredHeight: 104
        radius: Theme.radius
        color: areaKpi.containsMouse ? Theme.surfaceAlt : Theme.surface
        border.color: areaKpi.containsMouse && rota !== "" ? Theme.primary : Theme.border

        MouseArea {
            id: areaKpi
            anchors.fill: parent
            enabled: rota !== ""
            hoverEnabled: rota !== ""
            cursorShape: Qt.PointingHandCursor
            onClicked: tela.navegar(rota)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 4
            Text { text: rotulo; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.8 }
            Item { Layout.fillHeight: true }
            Text { text: valor; color: corValor; font.family: Theme.fontBase; font.pixelSize: 30; font.weight: Font.Bold }
            Text { text: nota; visible: nota !== ""; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
        }
    }

    component Painel: Rectangle {
        property string titulo: ""
        property string legenda: ""
        default property alias conteudo: slot.data
        Layout.fillHeight: true
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 18
                Text { text: titulo; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text { text: legenda; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
            Item {
                id: slot
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            KpiCard { rotulo: qsTr("Vendas hoje"); valor: App.formatarDinheiro(tela.kpis.vendasHoje || 0); corValor: Theme.success; nota: (tela.kpis.numVendasHoje || 0) + qsTr(" vendas") }
            KpiCard { rotulo: qsTr("Ticket médio"); valor: App.formatarDinheiro(tela.kpis.ticketMedio || 0) }
            KpiCard { rotulo: qsTr("Produtos em falta"); valor: "" + (tela.kpis.produtosEmFalta || 0); corValor: (tela.kpis.produtosEmFalta || 0) > 0 ? Theme.danger : Theme.text; nota: qsTr("abrir o Estoque"); rota: "estoque" }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Painel {
                Layout.horizontalStretchFactor: 2
                Layout.fillWidth: true
                titulo: qsTr("Mais vendidos")
                legenda: qsTr("7 dias")
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 0
                    Repeater {
                        model: tela.maisVendidos
                        delegate: RowLayout {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            Rectangle {
                                width: 22; height: 22; radius: 6; color: Theme.accentSoft
                                Text { anchors.centerIn: parent; text: (index + 1); color: Theme.primary; font.pixelSize: Theme.fontXs; font.weight: Font.Bold }
                            }
                            Text { text: modelData.nome; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight; Layout.topMargin: 8; Layout.bottomMargin: 8 }
                            Text { text: modelData.qtd; color: Theme.textMuted; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Text {
                        visible: tela.maisVendidos.length === 0
                        Layout.alignment: Qt.AlignCenter
                        text: qsTr("Sem vendas nos últimos 7 dias.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                    }
                }
            }

            Painel {
                Layout.horizontalStretchFactor: 1
                Layout.fillWidth: true
                titulo: qsTr("Financeiro")
                // A receber/A pagar/Saldo previsto é retaguarda: sem
                // ve_financeiro, esconder aqui também — senão trancar a tela
                // de Financeiro não adianta nada.
                visible: tela.veFinanceiro
                legenda: qsTr("em aberto")
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: Theme.spacingMd
                    // O fiado saiu daqui de propósito: ele vive na aba
                    // Clientes, que é onde se resolve. "Saldo previsto" foi
                    // junto — sem o a receber, ele seria um número enganoso.
                    ColumnLayout {
                        spacing: 2
                        Text { text: qsTr("A pagar"); color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
                        Text { text: App.formatarDinheiro(tela.fin.aPagar || 0); color: Theme.danger; font.pixelSize: Theme.fontXl; font.weight: Font.Bold }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
