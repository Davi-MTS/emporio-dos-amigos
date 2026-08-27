import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Financeiro: contas a pagar / a receber (com baixa), despesa avulsa e resumo.
Rectangle {
    id: tela
    color: Theme.background

    property var resumo: ({})

    Component.onCompleted: carregar()
    function carregar() { App.recarregarFinanceiro(); resumo = App.resumoFinanceiro(); }

    // Cartão de resumo.
    component ResumoCard: Rectangle {
        property string rotulo: ""
        property string valor: ""
        property color cor: Theme.text
        Layout.fillWidth: true
        Layout.preferredHeight: 84
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            spacing: 2
            Text { text: rotulo; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6; font.weight: Font.DemiBold }
            Item { Layout.fillHeight: true }
            Text { text: valor; color: cor; font.family: Theme.fontBase; font.pixelSize: Theme.fontXl; font.weight: Font.Bold }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Resumo
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            ResumoCard { rotulo: qsTr("A receber (aberto)"); valor: App.formatarDinheiro(tela.resumo.aReceber || 0); cor: Theme.success }
            ResumoCard { rotulo: qsTr("A pagar (aberto)"); valor: App.formatarDinheiro(tela.resumo.aPagar || 0); cor: Theme.danger }
            ResumoCard {
                rotulo: qsTr("Saldo previsto")
                valor: App.formatarDinheiro(tela.resumo.saldo || 0)
                cor: (tela.resumo.saldo || 0) >= 0 ? Theme.text : Theme.danger
            }
        }

        RowLayout {
            Layout.fillWidth: true
            SegmentedControl {
                id: tabs
                Layout.preferredWidth: 260
                Layout.preferredHeight: 42
                options: [qsTr("A pagar"), qsTr("A receber")]
            }
            Item { Layout.fillWidth: true }
            AppButton { kind: "accent"; text: qsTr("＋ Nova despesa"); onClicked: despesaDialog.abrir() }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            // ---- A pagar ----
            Rectangle {
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                clip: true
                ListView {
                    id: listaPagar
                    anchors.fill: parent
                    clip: true
                    model: App.contasPagar
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: rp
                        required property int idConta
                        required property string descricao
                        required property string fornecedor
                        required property var valor
                        required property string vencimento
                        required property bool vencida
                        width: ListView.view.width
                        height: 54
                        color: "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: rp.descricao; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: rp.fornecedor + (rp.vencimento ? " · vence " + rp.vencimento : ""); color: rp.vencida ? Theme.danger : Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text { text: App.formatarDinheiro(rp.valor); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                            AppButton { kind: "default"; text: qsTr("Pagar"); onClicked: pagarDialog.abrir(rp.idConta, rp.valor, rp.descricao) }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    }
                    Label { anchors.centerIn: parent; visible: listaPagar.count === 0; text: qsTr("Nenhuma conta a pagar em aberto."); color: Theme.textMuted }
                }
            }

            // ---- A receber ----
            Rectangle {
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                clip: true
                ListView {
                    id: listaReceber
                    anchors.fill: parent
                    clip: true
                    model: App.contasReceber
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: rr
                        required property int idConta
                        required property string cliente
                        required property var valor
                        required property string vencimento
                        required property bool vencida
                        width: ListView.view.width
                        height: 54
                        color: "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: rr.cliente; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: rr.vencimento ? qsTr("vence ") + rr.vencimento : qsTr("fiado"); color: rr.vencida ? Theme.danger : Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text { text: App.formatarDinheiro(rr.valor); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                            AppButton { kind: "default"; text: qsTr("Receber"); onClicked: receberDialog.abrir(rr.idConta, rr.valor, rr.cliente) }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    }
                    Label { anchors.centerIn: parent; visible: listaReceber.count === 0; text: qsTr("Nenhuma conta a receber em aberto."); color: Theme.textMuted }
                }
            }
        }
    }

    // Nova despesa (conta a pagar avulsa)
    AppDialog {
        id: despesaDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 480
        padding: Theme.spacingLg
        function abrir() { dDesc.text = ""; dValor.text = ""; dVenc.text = ""; dErro.text = ""; open(); }
        title: qsTr("Nova despesa")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            FormField { label: qsTr("Descrição"); Layout.fillWidth: true; AppTextField { id: dDesc; width: parent.width; placeholderText: qsTr("ex.: aluguel, energia") } }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                FormField { label: qsTr("Valor"); Layout.fillWidth: true; AppTextField { id: dValor; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" } }
                FormField { label: qsTr("Vencimento"); Layout.fillWidth: true; AppTextField { id: dVenc; width: parent.width; placeholderText: qsTr("AAAA-MM-DD") } }
            }
            Label { id: dErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"; text: qsTr("Adicionar")
                    onClicked: {
                        if (App.criarDespesa(dDesc.text, dValor.text, dVenc.text)) { tela.carregar(); despesaDialog.close(); }
                        else dErro.text = App.ultimoErro();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: despesaDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Pagar uma conta. Dinheiro sai da gaveta (sangria no caixa aberto).
    AppDialog {
        id: pagarDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        property int contaId: 0
        property int valorConta: 0
        property string descricao: ""
        function abrir(id, valor, desc) {
            contaId = id; valorConta = valor; descricao = desc || "";
            pgForma.currentIndex = 0;
            pgErro.text = "";
            open();
        }
        title: qsTr("Pagar conta")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true; elide: Text.ElideRight
                text: pagarDialog.descricao + qsTr(" · ") + App.formatarDinheiro(pagarDialog.valorConta)
                color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold
            }
            FormField {
                label: qsTr("Forma de pagamento")
                Layout.fillWidth: true
                AppComboBox {
                    id: pgForma
                    width: parent.width
                    model: [
                        { l: qsTr("Dinheiro (sai da gaveta)"), v: "dinheiro" },
                        { l: qsTr("Pix / transferência"),      v: "pix" },
                        { l: qsTr("Débito"),                   v: "debito" },
                        { l: qsTr("Crédito"),                  v: "credito" }
                    ]
                    textRole: "l"
                    valueRole: "v"
                    Component.onCompleted: currentIndex = 0
                }
            }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: qsTr("Se for em dinheiro, o valor é lançado como sangria (retirada) no caixa aberto, para o fechamento bater.")
                color: Theme.textMuted; font.pixelSize: Theme.fontXs
            }
            Label { id: pgErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Confirmar pagamento")
                    onClicked: {
                        if (App.pagarConta(pagarDialog.contaId, pgForma.currentValue)) { tela.carregar(); pagarDialog.close(); }
                        else pgErro.text = App.ultimoErro();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: pagarDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Receber uma conta (parcial ou total). Dinheiro entra no caixa aberto.
    AppDialog {
        id: receberDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        property int contaId: 0
        property int valorConta: 0
        property string clienteNome: ""
        function abrir(id, valor, cliente) {
            contaId = id; valorConta = valor; clienteNome = cliente || "";
            rcValor.text = App.formatarValor(valor);
            rcForma.currentIndex = 0;
            rcErro.text = "";
            open();
        }
        title: qsTr("Receber conta")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: receberDialog.clienteNome + qsTr(" · valor: ") + App.formatarDinheiro(receberDialog.valorConta)
                color: Theme.textMuted; font.pixelSize: Theme.fontSm
                Layout.fillWidth: true; elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                FormField {
                    label: qsTr("Valor recebido")
                    Layout.fillWidth: true
                    AppTextField { id: rcValor; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" }
                }
                FormField {
                    label: qsTr("Forma")
                    Layout.fillWidth: true
                    AppComboBox {
                        id: rcForma
                        width: parent.width
                        model: [
                            { l: qsTr("Dinheiro"), v: "dinheiro" },
                            { l: qsTr("Pix"),      v: "pix" },
                            { l: qsTr("Débito"),   v: "debito" },
                            { l: qsTr("Crédito"),  v: "credito" }
                        ]
                        textRole: "l"
                        valueRole: "v"
                        Component.onCompleted: currentIndex = 0
                    }
                }
            }
            Text {
                text: qsTr("Reduza o valor para pagamento parcial. Só o que for em dinheiro entra na gaveta do caixa.")
                color: Theme.textMuted; font.pixelSize: Theme.fontXs
                Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
            Label { id: rcErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Confirmar recebimento")
                    onClicked: {
                        var r = App.receberContaValor(receberDialog.contaId, rcValor.text, rcForma.currentValue);
                        if (r.ok) { tela.carregar(); receberDialog.close(); }
                        else rcErro.text = r.erro;
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: receberDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
