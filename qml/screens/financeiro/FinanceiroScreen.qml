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

    function nomeForma(f) {
        if (f === "dinheiro") return qsTr("dinheiro da gaveta");
        if (f === "pix")      return qsTr("pix / transferência");
        if (f === "debito")   return qsTr("cartão de débito");
        if (f === "credito")  return qsTr("cartão de crédito");
        return f;
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

        // Flow, e não RowLayout: com a janela restaurada os quatro controles não
        // cabem lado a lado e o RowLayout empurrava a coluna inteira para fora
        // da tela (a lista ficava 79 px mais larga que a janela). Aqui eles
        // simplesmente passam para a linha de baixo.
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            SegmentedControl {
                id: tabs
                width: 260
                height: 42
                options: [qsTr("A pagar"), qsTr("A receber")]
            }
            // Sem isto a lista só mostrava contas abertas: uma conta paga por
            // engano sumia da tela e não havia como achá-la para desfazer.
            ToggleButton {
                id: verPagas
                visible: tabs.currentIndex === 0
                text: qsTr("Mostrar já pagas")
                onCheckedChanged: { App.mostrarContasPagas(checked); tela.carregar(); }
            }
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
                        required property string status
                        required property string pagoEm
                        required property string formaPagamento
                        required property bool avulsa
                        readonly property bool paga: status === "paga"
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
                                Text {
                                    Layout.fillWidth: true
                                    text: rp.descricao
                                    color: rp.paga ? Theme.textMuted : Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: rp.paga
                                          ? qsTr("PAGA em ") + rp.pagoEm
                                            + (rp.formaPagamento.length > 0
                                               ? qsTr(" · saiu de: ") + tela.nomeForma(rp.formaPagamento)
                                               : qsTr(" · forma não registrada"))
                                          : rp.fornecedor + (rp.vencimento ? qsTr(" · vence ") + rp.vencimento : "")
                                    color: rp.paga ? Theme.success : (rp.vencida ? Theme.danger : Theme.textMuted)
                                    font.pixelSize: Theme.fontXs
                                }
                            }
                            Text {
                                text: App.formatarDinheiro(rp.valor)
                                color: rp.paga ? Theme.textMuted : Theme.text
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                            }
                            AppButton {
                                visible: !rp.paga
                                kind: "default"
                                text: qsTr("Pagar")
                                onClicked: pagarDialog.abrir(rp.idConta, rp.valor, rp.descricao)
                            }
                            AppButton {
                                visible: rp.paga
                                kind: "ghost"
                                text: qsTr("Desfazer")
                                onClicked: estornoDialog.abrir(rp.idConta, rp.valor, rp.descricao, rp.formaPagamento)
                            }
                            // Só despesa digitada à mão e ainda em aberto. Conta de
                            // compra tem mercadoria atrás dela, e conta paga tem
                            // dinheiro que já saiu — essas não somem.
                            AppButton {
                                visible: rp.avulsa && !rp.paga
                                kind: "perigo"
                                text: qsTr("Excluir")
                                onClicked: excluirDialog.abrir(rp.idConta, rp.valor, rp.descricao)
                            }
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
                FormField { label: qsTr("Vencimento"); Layout.fillWidth: true; AppDateField { id: dVenc; width: parent.width } }
            }
            Label { id: dErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"; text: qsTr("Adicionar")
                    onClicked: {
                        if (dVenc.incompleto) {
                            dErro.text = qsTr("Vencimento inválido — use dd/mm/aaaa.");
                        } else if (App.criarDespesa(dDesc.text, dValor.text, dVenc.iso)) {
                            tela.carregar(); despesaDialog.close();
                        }
                        else dErro.text = App.ultimoErro();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: despesaDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Pagar uma conta. A tela precisa dizer, ANTES de confirmar, de onde o
    // dinheiro sai e como a gaveta fica depois — antes era uma linha cinza.
    AppDialog {
        id: pagarDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 520
        padding: Theme.spacingLg
        property int contaId: 0
        property int valorConta: 0
        property string descricao: ""
        readonly property var formas: ["dinheiro", "pix", "debito", "credito"]
        property var efeito: ({})

        function abrir(id, valor, desc) {
            contaId = id; valorConta = valor; descricao = desc || "";
            formaTabs.currentIndex = 0;
            pgErro.text = "";
            recalcular();
            open();
        }
        function formaAtual() { return formas[formaTabs.currentIndex]; }
        function recalcular() { efeito = App.efeitoDoPagamento(formaAtual(), valorConta); }

        title: qsTr("Pagar conta")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd

            // O que está sendo pago, em destaque.
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusSm
                color: Theme.surfaceAlt
                border.color: Theme.border
                implicitHeight: cabPag.implicitHeight + 2 * Theme.spacingMd
                ColumnLayout {
                    id: cabPag
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: pagarDialog.descricao
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                    }
                    Text {
                        text: App.formatarDinheiro(pagarDialog.valorConta)
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.fontXl
                        font.weight: Font.Bold
                    }
                }
            }

            Text {
                text: qsTr("De onde sai o dinheiro?")
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            SegmentedControl {
                id: formaTabs
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                options: [qsTr("Gaveta"), qsTr("Pix"), qsTr("Débito"), qsTr("Crédito")]
                onCurrentIndexChanged: pagarDialog.recalcular()
            }

            // A consequência em português, com o antes e o depois da gaveta.
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusSm
                color: Theme.surface
                border.color: (pagarDialog.efeito.alerta || "").length > 0 ? Theme.warning : Theme.border
                implicitHeight: efeitoCol.implicitHeight + 2 * Theme.spacingMd
                ColumnLayout {
                    id: efeitoCol
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: 4
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: pagarDialog.efeito.sai || ""
                        color: Theme.text
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: pagarDialog.efeito.gavetaAgora !== undefined
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Gaveta agora")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                        Text {
                            text: App.formatarDinheiro(pagarDialog.efeito.gavetaAgora || 0)
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: pagarDialog.efeito.gavetaDepois !== undefined
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Gaveta depois de pagar")
                            color: Theme.text
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: App.formatarDinheiro(pagarDialog.efeito.gavetaDepois || 0)
                            color: (pagarDialog.efeito.gavetaDepois || 0) < 0 ? Theme.danger : Theme.primary
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.Bold
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        visible: (pagarDialog.efeito.alerta || "").length > 0
                        text: "⚠  " + (pagarDialog.efeito.alerta || "")
                        color: Theme.warning
                        font.pixelSize: Theme.fontXs
                        font.weight: Font.DemiBold
                    }
                }
            }

            Label { id: pgErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Confirmar pagamento")
                    onClicked: {
                        if (App.pagarConta(pagarDialog.contaId, pagarDialog.formaAtual())) {
                            tela.carregar();
                            pagarDialog.close();
                        } else {
                            pgErro.text = App.ultimoErro();
                        }
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: pagarDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Excluir uma despesa lançada por engano.
    AppDialog {
        id: excluirDialog
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
            excErro.text = "";
            open();
        }
        title: qsTr("Excluir despesa")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Apagar de vez: ") + excluirDialog.descricao + " · "
                      + App.formatarDinheiro(excluirDialog.valorConta)
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("A despesa some da lista e não fica no histórico. Como ela ainda não foi paga, nada muda no caixa.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            Label { id: excErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "perigo"
                    text: qsTr("Excluir despesa")
                    onClicked: {
                        var r = App.excluirDespesa(excluirDialog.contaId);
                        if (r.ok) { tela.carregar(); excluirDialog.close(); }
                        else excErro.text = r.erro;
                    }
                }
                AppButton { kind: "default"; text: qsTr("Voltar"); onClicked: excluirDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Desfazer um pagamento lançado por engano.
    AppDialog {
        id: estornoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 480
        padding: Theme.spacingLg
        property int contaId: 0
        property int valorConta: 0
        property string descricao: ""
        property string forma: ""
        function abrir(id, valor, desc, f) {
            contaId = id; valorConta = valor; descricao = desc || ""; forma = f || "";
            estErro.text = "";
            open();
        }
        title: qsTr("Desfazer o pagamento")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("A conta volta para a lista de contas a pagar: ")
                      + estornoDialog.descricao + " · "
                      + App.formatarDinheiro(estornoDialog.valorConta)
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: estornoDialog.forma === "dinheiro"
                      ? qsTr("Como saiu da gaveta, o dinheiro volta para o caixa aberto agora, lançado como suprimento com o motivo escrito. Se o caixa estiver fechado, a conta reabre mas o dinheiro não volta sozinho.")
                      : (estornoDialog.forma.length > 0
                         ? qsTr("Saiu por ") + tela.nomeForma(estornoDialog.forma)
                           + qsTr(": o caixa não é tocado. Se a transferência já foi feita no banco, desfaça lá também.")
                         : qsTr("Não há registro de como esta conta foi paga, então o caixa não será tocado."))
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            Label { id: estErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Desfazer pagamento")
                    onClicked: {
                        var r = App.estornarPagamento(estornoDialog.contaId);
                        if (r.ok) {
                            tela.carregar();
                            estornoDialog.close();
                            avisoEstornoTexto.text = r.aviso;
                            avisoEstorno.open();
                        } else {
                            estErro.text = r.erro;
                        }
                    }
                }
                AppButton { kind: "default"; text: qsTr("Voltar"); onClicked: estornoDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // O estorno precisa dizer o que aconteceu com o dinheiro.
    AppDialog {
        id: avisoEstorno
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 420
        padding: Theme.spacingLg
        title: qsTr("Pagamento desfeito")
        standardButtons: Dialog.Ok
        contentItem: ColumnLayout {
            Text {
                id: avisoEstornoTexto
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.text
                font.pixelSize: Theme.fontSm
            }
        }
    }

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
