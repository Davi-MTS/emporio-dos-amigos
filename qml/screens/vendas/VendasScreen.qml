import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Histórico de vendas: conferir o que foi vendido e cancelar uma venda errada.
// O cancelamento devolve o estoque, cancela o fiado e estorna o dinheiro.
Rectangle {
    id: tela
    color: Theme.background

    property int dias: 0
    readonly property bool podeCancelar: {
        var p = (App.usuarioAtual && App.usuarioAtual.permissoes) ? App.usuarioAtual.permissoes : ({});
        return p.tudo === true || p.pode_cancelar_venda === true;
    }

    Component.onCompleted: carregar()
    function carregar() { App.recarregarVendas(dias); }
    function fmtData(iso) {
        if (!iso || iso.length === 0) return "—";
        var d = new Date(iso.replace(" ", "T"));
        return isNaN(d) ? iso : Qt.formatDateTime(d, "dd/MM HH:mm");
    }
    function nomeFormas(f) {
        if (!f || !f.length) return "—";
        return f.split(", ").map(function (x) {
            return x.charAt(0).toUpperCase() + x.slice(1);
        }).join(", ");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Período
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            SegmentedControl {
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

        // Lista
        Rectangle {
            id: caixaLista
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            clip: true

            // Colunas somem quando a janela encolhe (evita sobreposição).
            readonly property bool mostrarCliente: width > 620
            readonly property bool mostrarFormas: width > 480
            readonly property bool mostrarItens: width > 380

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
                        Text { text: qsTr("Venda"); Layout.preferredWidth: 120; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { visible: caixaLista.mostrarCliente; text: qsTr("Cliente"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { visible: caixaLista.mostrarFormas; text: qsTr("Pagamento"); Layout.preferredWidth: 150; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { visible: caixaLista.mostrarItens; text: qsTr("Itens"); Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Total"); Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Item { Layout.preferredWidth: 96 }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                ListView {
                    id: lista
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.vendas
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: linha
                        required property int idVenda
                        required property string dataVenda
                        required property string cliente
                        required property string status
                        required property string formas
                        required property var total
                        required property int numItens
                        required property string motivo
                        readonly property bool cancelada: linha.status === "cancelada"

                        width: ListView.view.width
                        height: 52
                        color: "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            spacing: Theme.spacingSm

                            ColumnLayout {
                                Layout.preferredWidth: 120
                                spacing: 0
                                Text {
                                    text: "#" + linha.idVenda
                                    color: linha.cancelada ? Theme.textMuted : Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                    font.strikeout: linha.cancelada
                                }
                                Text { text: tela.fmtData(linha.dataVenda); color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            ColumnLayout {
                                visible: caixaLista.mostrarCliente
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: linha.cliente; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                                Text {
                                    visible: linha.cancelada && linha.motivo.length > 0
                                    text: qsTr("cancelada: ") + linha.motivo
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                    color: Theme.danger; font.pixelSize: Theme.fontXs
                                }
                            }
                            Text { visible: caixaLista.mostrarFormas; text: tela.nomeFormas(linha.formas); Layout.preferredWidth: 150; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { visible: caixaLista.mostrarItens; text: linha.numItens; Layout.preferredWidth: 60; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                            Text {
                                text: App.formatarDinheiro(linha.total)
                                Layout.preferredWidth: 110
                                horizontalAlignment: Text.AlignRight
                                color: linha.cancelada ? Theme.textMuted : Theme.text
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                                font.strikeout: linha.cancelada
                            }
                            RowLayout {
                                Layout.preferredWidth: 96
                                spacing: 4
                                AppButton {
                                    kind: "ghost"
                                    text: qsTr("Ver")
                                    implicitWidth: 44
                                    onClicked: detalheDialog.abrir(linha.idVenda, linha.total, linha.cancelada)
                                }
                                Rectangle {
                                    visible: linha.cancelada
                                    implicitWidth: 48; implicitHeight: 20; radius: 6
                                    color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.15)
                                    Text { anchors.centerIn: parent; text: qsTr("cancel."); color: Theme.danger; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                                }
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        text: qsTr("Nenhuma venda no período.")
                        color: Theme.textMuted
                    }
                }
            }
        }
    }

    // Detalhe da venda + cancelamento
    AppDialog {
        id: detalheDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 520
        padding: Theme.spacingLg
        property int vendaId: 0
        property int total: 0
        property bool cancelada: false
        property var itens: []
        function abrir(id, tot, canc) {
            vendaId = id; total = tot; cancelada = canc;
            itens = App.itensDaVenda(id);
            motivoField.text = "";
            erroDet.text = "";
            open();
        }
        title: qsTr("Venda #") + detalheDialog.vendaId
        contentItem: ScrollView {
            id: rolVenda
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            width: rolVenda.availableWidth
            spacing: Theme.spacingMd

            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusSm
                color: Theme.surfaceAlt
                border.color: Theme.border
                implicitHeight: itensCol.implicitHeight + 2 * Theme.spacingSm
                ColumnLayout {
                    id: itensCol
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    spacing: 2
                    Repeater {
                        model: detalheDialog.itens
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: modelData.produto; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                                Text { text: modelData.embalagem + " · " + modelData.qtd; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text {
                                text: App.formatarDinheiro(modelData.precoUnit)
                                color: Theme.textMuted; font.pixelSize: Theme.fontSm
                            }
                        }
                    }
                    Label {
                        visible: detalheDialog.itens.length === 0
                        Layout.fillWidth: true
                        text: qsTr("Sem itens registrados.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Total da venda"); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text { text: App.formatarDinheiro(detalheDialog.total); color: Theme.primary; font.pixelSize: Theme.fontLg; font.weight: Font.Bold }
            }

            // Cancelamento
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; visible: !detalheDialog.cancelada }
            Text {
                visible: detalheDialog.cancelada
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Esta venda já foi cancelada.")
                color: Theme.danger
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                visible: !detalheDialog.cancelada && !tela.podeCancelar
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Você não tem permissão para cancelar vendas.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            FormField {
                visible: !detalheDialog.cancelada && tela.podeCancelar
                label: qsTr("Motivo do cancelamento")
                Layout.fillWidth: true
                AppTextField { id: motivoField; width: parent.width; placeholderText: qsTr("ex.: cliente desistiu, item errado") }
            }
            Text {
                visible: !detalheDialog.cancelada && tela.podeCancelar
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Cancelar devolve os itens ao estoque, cancela o fiado desta venda e estorna o dinheiro no caixa. A venda continua no histórico, marcada como cancelada.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
            Label { id: erroDet; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }

            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    visible: !detalheDialog.cancelada && tela.podeCancelar
                    kind: "accent"
                    text: qsTr("Cancelar esta venda")
                    onClicked: {
                        var r = App.cancelarVenda(detalheDialog.vendaId, motivoField.text);
                        if (r.ok) { detalheDialog.close(); tela.carregar(); }
                        else erroDet.text = r.erro;
                    }
                }
                AppButton { kind: "default"; text: qsTr("Fechar"); onClicked: detalheDialog.close() }
                Item { Layout.fillWidth: true }
            }
            }
        }
    }
}
