import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Tela de Estoque: lista com status + diálogo de Entrada de mercadoria e
// Inventário. Quantidades sempre em unidade base; entrada aceita embalagem.
Rectangle {
    id: tela
    color: Theme.background

    readonly property var _perms: (App.usuarioAtual && App.usuarioAtual.permissoes)
                                  ? App.usuarioAtual.permissoes : ({})
    readonly property bool podeAjustarEstoque: _perms.tudo === true || _perms.ajusta_estoque === true
    readonly property bool podeReceberMercadoria: _perms.tudo === true || _perms.recebe_mercadoria === true

    readonly property int colLoc: 150
    readonly property int colQtd: 96
    readonly property int colMin: 108
    readonly property int colCusto: 116
    readonly property int colStatus: 96

    readonly property bool podeMovimentar: podeReceberMercadoria || podeAjustarEstoque

    // Filtro por situação. As chaves são as mesmas do selo da coluna Status; a
    // regra de quem é "zerado" ou "baixo" mora só no model (C++).
    readonly property var chavesFiltro: ["", "zerado", "baixo", "ok"]
    readonly property var nomesFiltro: ({ "": qsTr("Todos"), "zerado": qsTr("Zerados"),
                                          "baixo": qsTr("Baixo"), "ok": qsTr("OK") })
    readonly property var contagem: App.estoque.contagem
    function rotuloFiltro(chave) {
        var n = tela.contagem[chave === "" ? "todos" : chave];
        return tela.nomesFiltro[chave] + "  " + (n !== undefined ? n : 0);
    }

    // O filtro vive no model, que é um só para o app: sair da tela com "Zerados"
    // ligado faria a próxima visita abrir escondendo produtos.
    Component.onDestruction: App.estoque.filtroStatus = ""

    function abrirMov(produtoId) {
        if (!podeMovimentar)
            return;   // sem permissão o diálogo não teria nenhuma ação válida
        movDialog.produtoId = produtoId;
        movDialog.embalagens = App.embalagensDe(produtoId);
        movDialog.atual = App.itemEstoque(produtoId);
        movDialog.abrir();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Toolbar. Flow, e não RowLayout: busca + filtro + dica não cabem lado a
        // lado na janela restaurada, e aqui passam para a linha de baixo em vez
        // de empurrar a lista para fora da tela.
        Flow {
            id: barraEstoque
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            AppTextField {
                id: buscaField
                width: Math.min(260, barraEstoque.width)
                placeholderText: qsTr("Buscar produto…")
                onTextChanged: App.recarregarEstoque(text)
            }
            // Urgência primeiro: o que zerou, depois o que está acabando.
            SegmentedControl {
                id: filtroStatus
                objectName: "filtroStatusEstoque"
                width: Math.min(400, barraEstoque.width)
                height: 40
                options: tela.chavesFiltro.map(function (c) { return tela.rotuloFiltro(c); })
                currentIndex: Math.max(0, tela.chavesFiltro.indexOf(App.estoque.filtroStatus))
                onCurrentIndexChanged: App.estoque.filtroStatus = tela.chavesFiltro[currentIndex]
            }
            Label {
                height: 40
                verticalAlignment: Text.AlignVCenter
                text: tela.podeAjustarEstoque
                      ? qsTr("Clique num produto para dar entrada ou inventariar")
                      : (tela.podeReceberMercadoria
                         ? qsTr("Clique num produto para dar entrada de mercadoria")
                         : qsTr("Consulta apenas — seu usuário não movimenta estoque"))
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
        }

        // Lista
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Cabeçalho
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    color: Theme.surfaceAlt
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingSm
                        Text { text: qsTr("Produto"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Localização"); Layout.preferredWidth: tela.colLoc; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Qtd atual"); Layout.preferredWidth: tela.colQtd; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Mínimo"); Layout.preferredWidth: tela.colMin; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Custo médio"); Layout.preferredWidth: tela.colCusto; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Status"); Layout.preferredWidth: tela.colStatus; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                ListView {
                    id: lista
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.estoque
                    ScrollBar.vertical: ScrollBar {}

                    delegate: ItemDelegate {
                        id: linha
                        required property int idProduto
                        required property string nome
                        required property string localizacao
                        required property var quantidade
                        required property var minimo
                        required property var custoMedio
                        required property string unidadeBase
                        required property string status
                        required property bool temFoto

                        width: ListView.view.width
                        height: 52
                        leftPadding: Theme.spacingMd
                        rightPadding: Theme.spacingMd
                        onClicked: tela.abrirMov(idProduto)

                        contentItem: RowLayout {
                            spacing: Theme.spacingSm
                            FotoProduto {
                                produtoId: linha.idProduto
                                temFoto: linha.temFoto
                                nome: linha.nome
                                lado: 32
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 0
                                Text {
                                    text: linha.nome
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                    color: Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                }
                            }
                            Text { Layout.preferredWidth: tela.colLoc; text: linha.localizacao; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                            Text { Layout.preferredWidth: tela.colQtd; text: linha.quantidade + " " + linha.unidadeBase; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                            // Com a unidade junto, igual à "Qtd atual". Um "5" solo ao lado de
                            // "600 ml" se lê como 5 garrafas — e são 5 ml.
                            Text {
                                Layout.preferredWidth: tela.colMin
                                text: linha.minimo > 0 ? (linha.minimo + " " + linha.unidadeBase) : "—"
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMd
                            }
                            Text { Layout.preferredWidth: tela.colCusto; text: App.formatarDinheiro(linha.custoMedio); horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                            Item {
                                Layout.preferredWidth: tela.colStatus
                                implicitHeight: 22
                                StatusBadge { status: linha.status; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                    }

                    // Lista vazia com filtro ou busca ligados NÃO é "nenhum produto
                    // cadastrado" — dizer isso faria o dono achar que perdeu o cadastro.
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 2 * Theme.spacingLg
                        visible: lista.count === 0
                        wrapMode: Text.WordWrap
                        text: {
                            var f = App.estoque.filtroStatus;
                            if (f === "zerado") return qsTr("Nenhum produto zerado.");
                            if (f === "baixo")  return qsTr("Nenhum produto com estoque baixo.");
                            if (f === "ok")     return qsTr("Nenhum produto com estoque OK.");
                            if (buscaField.text.trim().length > 0)
                                return qsTr("Nenhum produto encontrado para “%1”.").arg(buscaField.text.trim());
                            return qsTr("Nenhum produto cadastrado.\nCadastre em Produtos para controlar o estoque.");
                        }
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.textMuted
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------------- Diálogo
    AppDialog {
        id: movDialog
        objectName: "movDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 480
        modal: true
        padding: Theme.spacingLg

        property int produtoId: 0
        property var embalagens: []
        property var atual: ({})
        // Custo fora do normal na entrada: só avisa. O primeiro "Confirmar"
        // mostra o aviso; o segundo grava. Mudar custo/embalagem zera.
        readonly property var avisoCusto: App.avaliarCusto(produtoId, embCombo.currentValue !== undefined ? embCombo.currentValue : 0, custoField.text)
        readonly property bool temAvisoCusto: avisoCusto.nivel !== undefined && avisoCusto.nivel.length > 0
        property bool custoConferido: false
        onAvisoCustoChanged: custoConferido = false

        function abrir() {
            erro.text = "";
            embCombo.currentIndex = 0;
            qtdSpin.value = 1;
            custoField.text = "";
            obsField.text = "";
            validadeField.text = "";
            loteField.text = "";
            contagemSpin.value = atual.quantidade !== undefined ? atual.quantidade : 0;
            motivoField.text = "";
            retEmbCombo.currentIndex = 0;
            retQtdSpin.value = 1;
            retMotivoField.text = "";
            tabs.currentIndex = 0;
            custoConferido = false;
            open();
        }


        function _fatorSel() {
            var e = embalagens[embCombo.currentIndex];
            return e && e.fator ? e.fator : 1;
        }

        title: atual.nome !== undefined ? atual.nome : qsTr("Movimentar estoque")

        contentItem: ScrollView {
            id: rolMov
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            width: rolMov.availableWidth
            spacing: Theme.spacingMd

            // Situação atual
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radiusSm
                color: Theme.surfaceAlt
                border.color: Theme.border
                implicitHeight: infoRow.implicitHeight + 2 * Theme.spacingMd
                RowLayout {
                    id: infoRow
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: Theme.spacingLg
                    Column {
                        Text { text: qsTr("Em estoque"); color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
                        Text {
                            text: (movDialog.atual.quantidade !== undefined ? movDialog.atual.quantidade : 0)
                                  + " " + (movDialog.atual.unidadeBase !== undefined ? movDialog.atual.unidadeBase : "")
                            color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold
                        }
                    }
                    Column {
                        Text { text: qsTr("Custo médio"); color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
                        Text {
                            text: App.formatarDinheiro(movDialog.atual.custoMedio !== undefined ? movDialog.atual.custoMedio : 0)
                            color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }

            // Entrada (receber mercadoria) é do dia a dia do balcão. Inventário e
            // Retirada mexem no saldo sem nota — ficam só para quem pode ajustar.
            // As abas restritas são as ÚLTIMAS da lista de propósito: assim os
            // índices de Entrada continuam valendo quando elas somem.
            SegmentedControl {
                id: tabs
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                visible: tela.podeAjustarEstoque
                options: tela.podeAjustarEstoque
                         ? [qsTr("Entrada"), qsTr("Inventário"), qsTr("Retirada")]
                         : [qsTr("Entrada")]
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: tabs.currentIndex

                // --- Entrada ---
                ColumnLayout {
                    spacing: Theme.spacingSm
                    FormField {
                        label: qsTr("Embalagem recebida")
                        Layout.fillWidth: true
                        AppComboBox {
                            id: embCombo
                            objectName: "embComboEntrada"
                            width: parent.width
                            model: movDialog.embalagens
                            textRole: "nome"
                            valueRole: "id"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd
                        FormField {
                            label: qsTr("Quantidade")
                            Layout.preferredWidth: 140
                            AppSpinBox {
                                id: qtdSpin
                                width: parent.width
                                from: 1; to: 1000000; value: 1
                            }
                        }
                        FormField {
                            label: qsTr("Custo por embalagem (opcional)")
                            Layout.fillWidth: true
                            AppTextField {
                                id: custoField
                                objectName: "custoEntrada"
                                width: parent.width
                                placeholderText: qsTr("ex.: 62,90 — vazio mantém o custo")
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                    Text {
                        objectName: "avisoCustoEntrada"
                        Layout.fillWidth: true
                        visible: movDialog.temAvisoCusto
                        text: movDialog.temAvisoCusto ? "⚠ " + movDialog.avisoCusto.mensagem : ""
                        color: Theme.warning
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        text: qsTr("Entra %1 %2 no estoque.")
                              .arg(qtdSpin.value * movDialog._fatorSel())
                              .arg(movDialog.atual.unidadeBase !== undefined ? movDialog.atual.unidadeBase : "")
                    }
                    // Validade por REMESSA: duas cargas do mesmo doce chegam com
                    // datas diferentes, e uma data única no cadastro do produto
                    // seria sobrescrita pela carga nova — a mercadoria velha
                    // sairia do radar justo por ser a que precisa girar antes.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        FormField {
                            label: qsTr("Validade (opcional)")
                            Layout.fillWidth: true
                            AppDateField {
                                id: validadeField
                                width: parent.width
                            }
                        }
                        FormField {
                            label: qsTr("Lote (opcional)")
                            Layout.preferredWidth: 130
                            AppTextField { id: loteField; width: parent.width }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: validadeField.incompleto
                               ? Theme.danger : Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        text: validadeField.text.trim().length === 0
                              ? qsTr("Informe só para o que vence em prazo curto (doces, salgadinhos). Fica na aba Vencimento.")
                              : ((validadeField.iso.length > 0)
                                 ? qsTr("Esta remessa entra com validade e passa a aparecer na aba Vencimento.")
                                 : qsTr("Data inválida — use dd/mm/aaaa."))
                    }

                    FormField {
                        label: qsTr("Observação (opcional)")
                        Layout.fillWidth: true
                        AppTextField { id: obsField; width: parent.width }
                    }
                }

                // --- Inventário ---
                ColumnLayout {
                    spacing: Theme.spacingSm
                    FormField {
                        label: qsTr("Contagem real (unidade base)")
                        Layout.preferredWidth: 180
                        AppSpinBox {
                            id: contagemSpin
                            width: parent.width
                            from: 0; to: 100000000; value: 0
                        }
                    }
                    FormField {
                        label: qsTr("Motivo")
                        Layout.fillWidth: true
                        AppTextField { id: motivoField; width: parent.width; placeholderText: qsTr("ex.: contagem mensal, quebra") }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.WordWrap
                        text: qsTr("Ajusta o estoque para o valor contado e registra a diferença.")
                    }
                }

                // --- Retirada (perda, quebra, consumo próprio) ---
                ColumnLayout {
                    spacing: Theme.spacingSm
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd
                        FormField {
                            label: qsTr("Embalagem")
                            Layout.fillWidth: true
                            AppComboBox {
                                id: retEmbCombo
                                width: parent.width
                                model: movDialog.embalagens
                                textRole: "nome"
                                valueRole: "id"
                            }
                        }
                        FormField {
                            label: qsTr("Quantidade")
                            Layout.preferredWidth: 140
                            AppSpinBox {
                                id: retQtdSpin
                                width: parent.width
                                from: 1; to: 1000000; value: 1
                            }
                        }
                    }
                    FormField {
                        label: qsTr("Motivo")
                        Layout.fillWidth: true
                        AppTextField { id: retMotivoField; width: parent.width; placeholderText: qsTr("ex.: quebra, vencido, consumo próprio") }
                    }
                    Text {
                        Layout.fillWidth: true
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.WordWrap
                        text: qsTr("Retira %1 %2 do estoque. Não altera o custo médio nem conta como venda.")
                              .arg(retQtdSpin.value * (movDialog.embalagens[retEmbCombo.currentIndex] ? movDialog.embalagens[retEmbCombo.currentIndex].fator : 1))
                              .arg(movDialog.atual.unidadeBase !== undefined ? movDialog.atual.unidadeBase : "")
                    }
                }
            }

            Label {
                id: erro
                objectName: "erroMovimento"
                Layout.fillWidth: true
                visible: text.length > 0
                color: Theme.danger
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingSm
                AppButton {
                    objectName: "confirmarMovimento"
                    kind: "accent"
                    text: tabs.currentIndex === 0 && movDialog.custoConferido
                          ? qsTr("Confirmar mesmo assim") : qsTr("Confirmar")
                    // Data escrita errada viraria entrada sem validade nenhuma,
                    // em silêncio — melhor barrar o botão.
                    enabled: tabs.currentIndex !== 0
                             || validadeField.text.trim().length === 0
                             || (validadeField.iso.length > 0)
                    onClicked: {
                        var ok;
                        if (tabs.currentIndex === 0 && movDialog.temAvisoCusto && !movDialog.custoConferido) {
                            movDialog.custoConferido = true;
                            erro.text = qsTr("Confira o custo");
                            return;
                        }
                        if (tabs.currentIndex === 0)
                            ok = tela.podeReceberMercadoria
                                 && App.registrarEntrada(movDialog.produtoId, embCombo.currentValue,
                                                      qtdSpin.value, custoField.text, obsField.text,
                                                      validadeField.iso, loteField.text);
                        else if (tabs.currentIndex === 1)
                            ok = App.registrarInventario(movDialog.produtoId, contagemSpin.value, motivoField.text);
                        else
                            ok = App.registrarRetirada(movDialog.produtoId, retEmbCombo.currentValue,
                                                       retQtdSpin.value, retMotivoField.text);
                        if (ok) movDialog.close();
                        else erro.text = App.ultimoErro();
                    }
                }
                AppButton {
                    kind: "default"
                    text: qsTr("Cancelar")
                    onClicked: movDialog.close()
                }
                Item { Layout.fillWidth: true }
            }
            }
        }
    }
}
