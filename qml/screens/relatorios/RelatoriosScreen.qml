import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Relatórios: faturamento, lucro, vendas por forma, mais vendidos, parados.
Rectangle {
    id: tela
    color: Theme.background

    property int dias: 0
    // Dia específico ("yyyy-MM-dd") quando a opção "Outro dia" está ligada.
    // Vazio = usa `dias` (hoje, 7 ou 30).
    property string dia: ""
    property var fat: ({})
    property var formas: []
    property var vendidos: []
    property var parados: []

    Component.onCompleted: carregar()
    function carregar() {
        if (dia.length > 0) {
            fat = App.relatorioFaturamentoDia(dia);
            formas = App.relatorioFormasDia(dia);
            vendidos = App.relatorioMaisVendidosDia(dia, 10);
            parados = App.relatorioProdutosParadosDia(dia);
            return;
        }
        fat = App.relatorioFaturamento(dias);
        formas = App.relatorioFormas(dias);
        vendidos = App.relatorioMaisVendidos(dias, 10);
        parados = App.relatorioProdutosParados(dias);
    }

    // "Outro dia" abre em ONTEM: hoje já tem botão próprio, e ontem é o dia que
    // mais se quer conferir.
    function ontemIso() {
        var d = new Date();
        d.setDate(d.getDate() - 1);
        return Qt.formatDate(d, "yyyy-MM-dd");
    }
    function hojeIso() { return Qt.formatDate(new Date(), "yyyy-MM-dd"); }

    // "2026-09-13" -> "sáb, 13/09/2026". A data é montada por inteiros em hora
    // local, então não escorrega de dia por causa do fuso.
    function rotuloDia(iso) {
        var m = ("" + iso).match(/^(\d{4})-(\d{2})-(\d{2})$/);
        if (!m) return qsTr("Escolher dia");
        var d = new Date(parseInt(m[1]), parseInt(m[2]) - 1, parseInt(m[3]));
        return Qt.locale("pt_BR").toString(d, "ddd, dd/MM/yyyy");
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
            // fillWidth + minimumWidth 0 + elide: sem os três, o título longo
            // ("Vendas por forma de pagamento") vira a largura MÍNIMA do painel.
            // Com a janela restaurada o painel encolhe abaixo disso e a lista de
            // dentro fica maior que o cartão, invadindo o painel do lado.
            Text {
                text: titulo
                Layout.margins: 16
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
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
                objectName: "periodoRelatorio"
                Layout.preferredWidth: 400
                Layout.preferredHeight: 42
                options: [qsTr("Hoje"), qsTr("7 dias"), qsTr("30 dias"), qsTr("Outro dia")]
                onCurrentIndexChanged: {
                    if (currentIndex === 3) {
                        if (tela.dia.length === 0)
                            tela.dia = tela.ontemIso();
                    } else {
                        tela.dia = "";
                        tela.dias = (currentIndex === 0) ? 0 : (currentIndex === 1 ? 7 : 30);
                    }
                    tela.carregar();
                }
            }
            // O dia se escolhe num calendário, não digitando: o botão mostra o dia
            // atual e abre o calendário logo abaixo.
            AppButton {
                id: botaoDia
                objectName: "diaRelatorio"
                kind: "default"
                visible: periodo.currentIndex === 3
                Layout.preferredHeight: 42
                text: tela.rotuloDia(tela.dia)
                onClicked: calendario.abrirEm(tela.dia)
                contentItem: RowLayout {
                    spacing: 8
                    AppIcon { name: "calendario"; size: 18; color: Theme.primary }
                    Text {
                        text: botaoDia.text
                        color: Theme.text
                        font: botaoDia.font
                    }
                }

                CalendarioPopup {
                    id: calendario
                    objectName: "calendarioRelatorio"
                    y: botaoDia.height + 6
                    maximo: tela.hojeIso()
                    onEscolhido: (iso) => {
                        tela.dia = iso;
                        tela.carregar();
                    }
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
                    Label { anchors.centerIn: parent; visible: tela.formas.length === 0; text: tela.dia.length > 0 ? qsTr("Sem vendas neste dia.") : qsTr("Sem vendas no período."); color: Theme.textMuted }
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
                    Label { anchors.centerIn: parent; visible: tela.vendidos.length === 0; text: tela.dia.length > 0 ? qsTr("Sem vendas neste dia.") : qsTr("Sem vendas no período."); color: Theme.textMuted }
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
                    Label { anchors.centerIn: parent; visible: tela.parados.length === 0; text: tela.dia.length > 0 ? qsTr("Todos venderam neste dia.") : qsTr("Todos venderam no período."); color: Theme.textMuted }
                }
            }
        }
    }
}
