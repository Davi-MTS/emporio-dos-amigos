import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Relatórios: faturamento, lucro, vendas por forma, mais vendidos, parados.
Rectangle {
    id: tela
    color: Theme.background

    property int dias: 0
    property var fat: ({})
    property var formas: []
    property var vendidos: []
    property var parados: []

    Component.onCompleted: carregar()
    function carregar() {
        fat = App.relatorioFaturamento(dias);
        formas = App.relatorioFormas(dias);
        vendidos = App.relatorioMaisVendidos(dias, 10);
        parados = App.relatorioProdutosParados(dias);
    }
    function nomeForma(f) {
        return f.charAt(0).toUpperCase() + f.slice(1);
    }

    component Card: Rectangle {
        property string rotulo: ""
        property string valor: ""
        property color cor: Theme.text
        Layout.fillWidth: true
        Layout.preferredHeight: 92
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            spacing: 2
            Text { text: rotulo; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
            Item { Layout.fillHeight: true }
            Text { text: valor; color: cor; font.family: Theme.fontBase; font.pixelSize: Theme.fontXl; font.weight: Font.Bold }
        }
    }

    component Painel: Rectangle {
        property string titulo: ""
        default property alias conteudo: slot.data
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        clip: true
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Text { text: titulo; Layout.margins: 16; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
            Item { id: slot; Layout.fillWidth: true; Layout.fillHeight: true }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Período
        RowLayout {
            spacing: Theme.spacingSm
            SegmentedControl {
                id: periodo
                Layout.preferredWidth: 300
                Layout.preferredHeight: 42
                options: [qsTr("Hoje"), qsTr("7 dias"), qsTr("30 dias")]
                onCurrentIndexChanged: {
                    tela.dias = (currentIndex === 0) ? 0 : (currentIndex === 1 ? 7 : 30);
                    tela.carregar();
                }
            }
            Item { Layout.fillWidth: true }
            AppButton { kind: "ghost"; text: qsTr("↻ Atualizar"); onClicked: tela.carregar() }
        }

        // Cartões
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            Card { rotulo: qsTr("Faturamento"); valor: App.formatarDinheiro(tela.fat.total || 0); cor: Theme.success }
            Card { rotulo: qsTr("Lucro estimado"); valor: App.formatarDinheiro(tela.fat.lucro || 0); cor: (tela.fat.lucro || 0) >= 0 ? Theme.text : Theme.danger }
            Card { rotulo: qsTr("Ticket médio"); valor: App.formatarDinheiro(tela.fat.ticket || 0) }
            Card { rotulo: qsTr("Nº de vendas"); valor: "" + (tela.fat.numVendas || 0) }
        }

        // Painéis
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Painel {
                titulo: qsTr("Vendas por forma de pagamento")
                ListView {
                    anchors.fill: parent
                    clip: true
                    model: tela.formas
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        width: ListView.view.width
                        height: 40
                        Text { text: tela.nomeForma(modelData.forma); Layout.fillWidth: true; Layout.leftMargin: 16; color: Theme.text; font.pixelSize: Theme.fontMd }
                        Text { text: App.formatarDinheiro(modelData.total); Layout.rightMargin: 16; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                    }
                    Label { anchors.centerIn: parent; visible: tela.formas.length === 0; text: qsTr("Sem vendas no período."); color: Theme.textMuted }
                }
            }

            Painel {
                titulo: qsTr("Mais vendidos")
                ListView {
                    anchors.fill: parent
                    clip: true
                    model: tela.vendidos
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        height: 40
                        Text { text: (index + 1) + ". " + modelData.nome; Layout.fillWidth: true; Layout.leftMargin: 16; color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight }
                        Text { text: modelData.qtd; Layout.rightMargin: 16; color: Theme.textMuted; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                    }
                    Label { anchors.centerIn: parent; visible: tela.vendidos.length === 0; text: qsTr("Sem vendas no período."); color: Theme.textMuted }
                }
            }

            Painel {
                titulo: qsTr("Produtos parados")
                ListView {
                    anchors.fill: parent
                    clip: true
                    model: tela.parados
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        width: ListView.view.width
                        height: 40
                        Text { text: modelData.nome; Layout.fillWidth: true; Layout.leftMargin: 16; color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight }
                        Text { text: qsTr("estq ") + modelData.estoque; Layout.rightMargin: 16; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                    }
                    Label { anchors.centerIn: parent; visible: tela.parados.length === 0; text: qsTr("Todos venderam no período."); color: Theme.textMuted }
                }
            }
        }
    }
}
