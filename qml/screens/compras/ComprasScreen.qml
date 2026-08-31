import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Compras: histórico + registro de nova compra (entra no estoque e no custo
// médio) e gestão de fornecedores.
Rectangle {
    id: tela
    color: Theme.background

    Component.onCompleted: { App.recarregarCompras(); App.recarregarFornecedores(); }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Text { text: qsTr("Histórico de compras"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            Item { Layout.fillWidth: true }
            AppButton { kind: "default"; text: qsTr("Fornecedores"); onClicked: fornecedoresDialog.open() }
            AppButton { kind: "accent"; text: qsTr("＋ Nova compra"); onClicked: novaCompraDialog.abrir() }
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
                        Text { text: qsTr("Fornecedor"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Data"); Layout.preferredWidth: 160; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Itens"); Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Total"); Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Status"); Layout.preferredWidth: 100; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
                ListView {
                    id: lista
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.compras
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        id: cRow
                        required property string fornecedor
                        required property string dataCompra
                        required property var total
                        required property string status
                        required property int numItens
                        required property string numeroNota
                        width: ListView.view.width
                        height: 46
                        color: "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: cRow.fornecedor; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { visible: cRow.numeroNota.length > 0; text: qsTr("NF ") + cRow.numeroNota; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text { text: cRow.dataCompra; Layout.preferredWidth: 160; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                            Text { text: cRow.numItens; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                            Text { text: App.formatarDinheiro(cRow.total); Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                            Text { text: cRow.status; Layout.preferredWidth: 100; color: Theme.success; font.pixelSize: Theme.fontSm }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        text: qsTr("Nenhuma compra registrada.")
                        color: Theme.textMuted
                    }
                }
            }
        }
    }

    // ======================= NOVA COMPRA =======================
    AppDialog {
        id: novaCompraDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 660
        padding: Theme.spacingLg
        property var fornecedores: []
        property int totalCompra: 0

        ListModel { id: itensModel }   // produtoId, nome, embList, embId, fator, qtd, custoTexto, validadeTexto
        ListModel { id: sugCompra }

        function abrir() {
            fornecedores = App.fornecedoresLista();
            itensModel.clear();
            sugCompra.clear();
            buscaProduto.text = "";
            gerarConta.checked = false;
            vencField.text = "";
            notaField.text = "";
            notaDataField.text = "";
            erroCompra.text = "";
            totalCompra = 0;
            fornCombo.currentIndex = 0;
            open();
        }
        function recomputar() {
            var s = 0;
            for (var i = 0; i < itensModel.count; i++) {
                var it = itensModel.get(i);
                var c = App.parseDinheiro(it.custoTexto);
                if (c < 0) c = 0;
                s += it.qtd * c;
            }
            totalCompra = s;
        }
        function adicionarProduto(item) {
            var embs = App.embalagensDe(item.produtoId);
            // Pré-preenche com o último custo conhecido (custo médio × fator da
            // embalagem). É só uma sugestão editável — agiliza recompras.
            var est = App.itemEstoque(item.produtoId);
            var fator = item.fator > 0 ? item.fator : 1;
            var custoIni = (est && est.custoMedio > 0)
                           ? App.formatarValor(est.custoMedio * fator) : "";
            // embListJson: o ListModel NÃO preserva arrays de objetos (viram
            // ListModel aninhado e o índice [] devolve undefined) — por isso a
            // escolha de "Caixa" não aplicava o fator. String JSON atravessa.
            itensModel.append({
                produtoId: item.produtoId, nome: item.nome,
                embListJson: JSON.stringify(embs), embId: item.embalagemId, fator: item.fator,
                qtd: 1, custoTexto: custoIni, validadeTexto: ""
            });
            recomputar();
        }
        // dd/mm/aaaa -> ISO. Devolve vazio quando não dá para entender.
        function validadeIso(texto) {
            var t = ("" + texto).trim();
            if (t.length === 0)
                return "";
            var m = t.match(/^(\d{1,2})[\/\-.](\d{1,2})[\/\-.](\d{4})$/);
            if (!m)
                return "";
            var d = new Date(parseInt(m[3]), parseInt(m[2]) - 1, parseInt(m[1]));
            if (isNaN(d.getTime()) || d.getMonth() !== parseInt(m[2]) - 1)
                return "";
            return Qt.formatDate(d, "yyyy-MM-dd");
        }

        function confirmar() {
            if (itensModel.count === 0) { erroCompra.text = qsTr("Adicione ao menos um item."); return; }
            if (vencField.incompleto || notaDataField.incompleto) {
                erroCompra.text = qsTr("Data inválida — use dd/mm/aaaa.");
                return;
            }
            var itens = [];
            for (var i = 0; i < itensModel.count; i++) {
                var it = itensModel.get(i);
                var c = App.parseDinheiro(it.custoTexto);
                // Custo é obrigatório: é ele que atualiza o custo médio e, portanto,
                // o lucro. Sem custo, a venda mostraria lucro cheio (custo zero).
                if (c <= 0) {
                    erroCompra.text = qsTr("Informe o custo de cada item — falta em: ") + it.nome;
                    return;
                }
                // Validade é opcional, mas se foi escrita tem que estar legível:
                // uma data que o sistema não entende viraria mercadoria sem
                // controle de vencimento, em silêncio.
                var validade = "";
                if (it.validadeTexto && it.validadeTexto.trim().length > 0) {
                    validade = novaCompraDialog.validadeIso(it.validadeTexto);
                    if (validade.length === 0) {
                        erroCompra.text = qsTr("Validade inválida em: ") + it.nome
                                        + qsTr(" — use dd/mm/aaaa.");
                        return;
                    }
                }
                itens.push({ produtoId: it.produtoId, embalagemId: it.embId, fator: it.fator,
                             qtd: it.qtd, custo: c, validade: validade });
            }
            var r = App.registrarCompra({
                fornecedorId: fornCombo.currentValue ? fornCombo.currentValue : 0,
                gerarContaPagar: gerarConta.checked,
                vencimento: vencField.iso,
                numeroNota: notaField.text,
                dataNota: notaDataField.iso,
                itens: itens
            });
            if (r.ok) novaCompraDialog.close();
            else erroCompra.text = r.erro;
        }

        title: qsTr("Nova compra")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd

            FormField {
                label: qsTr("Fornecedor")
                Layout.fillWidth: true
                AppComboBox {
                    id: fornCombo
                    width: parent.width
                    model: novaCompraDialog.fornecedores
                    textRole: "nome"
                    valueRole: "id"
                }
            }

            // Nota fiscal de origem (opcional).
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                FormField {
                    label: qsTr("Nº da nota fiscal")
                    Layout.fillWidth: true
                    AppTextField { id: notaField; width: parent.width; placeholderText: qsTr("opcional") }
                }
                FormField {
                    label: qsTr("Data da nota")
                    Layout.preferredWidth: 160
                    AppDateField { id: notaDataField; width: parent.width }
                }
            }

            // Busca de produto
            FormField {
                label: qsTr("Adicionar produto")
                Layout.fillWidth: true
                AppTextField {
                    id: buscaProduto
                    width: parent.width
                    placeholderText: qsTr("Digite o nome do produto…")
                    onTextChanged: {
                        sugCompra.clear();
                        if (text.trim().length >= 2) {
                            var l = App.buscarProdutosPorNome(text, false);
                            for (var i = 0; i < l.length; i++) sugCompra.append(l[i]);
                        }
                    }
                    Popup {
                        y: parent.height + 2
                        width: parent.width
                        padding: 4
                        visible: buscaProduto.activeFocus && buscaProduto.text.trim().length >= 2
                        closePolicy: Popup.NoAutoClose
                        background: Rectangle { radius: Theme.radiusSm; color: Theme.surface; border.color: Theme.border }
                        contentItem: ColumnLayout {
                            spacing: 2
                            ListView {
                                Layout.fillWidth: true
                                visible: sugCompra.count > 0
                                implicitHeight: Math.min(contentHeight, 200)
                                clip: true
                                model: sugCompra
                                delegate: ItemDelegate {
                                    width: ListView.view.width
                                    required property int index
                                    required property string nome
                                    contentItem: Text { text: nome; color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight }
                                    onClicked: {
                                        novaCompraDialog.adicionarProduto(sugCompra.get(index));
                                        buscaProduto.text = "";
                                        sugCompra.clear();
                                    }
                                }
                            }
                            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; visible: sugCompra.count > 0 }
                            // Cadastro rápido do produto direto na compra.
                            ItemDelegate {
                                Layout.fillWidth: true
                                contentItem: RowLayout {
                                    spacing: 8
                                    Text { text: "＋"; color: Theme.primary; font.pixelSize: Theme.fontLg; font.weight: Font.Bold }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("Cadastrar “") + buscaProduto.text.trim() + qsTr("” como novo produto")
                                        color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight
                                    }
                                }
                                onClicked: novoProdutoRapido.abrir(buscaProduto.text.trim())
                            }
                        }
                    }
                }
            }

            // Itens
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(itensCol.implicitHeight + 8, 240)
                radius: Theme.radiusSm
                color: Theme.surfaceAlt
                border.color: Theme.border
                visible: itensModel.count > 0
                ScrollView {
                    id: itensScroll
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    ColumnLayout {
                        id: itensCol
                        width: itensScroll.availableWidth
                        spacing: 4
                        Repeater {
                            model: itensModel
                            delegate: RowLayout {
                                id: compraRow
                                required property int index
                                required property string nome
                                required property string embListJson
                                required property int embId
                                required property int qtd
                                required property string custoTexto
                                required property string validadeTexto
                                readonly property var embList: JSON.parse(compraRow.embListJson && compraRow.embListJson.length ? compraRow.embListJson : "[]")
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                Text { text: compraRow.nome; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontSm; elide: Text.ElideRight }
                                AppComboBox {
                                    Layout.preferredWidth: 132
                                    model: compraRow.embList
                                    textRole: "nome"
                                    valueRole: "id"
                                    Component.onCompleted: currentIndex = indexOfValue(compraRow.embId)
                                    onActivated: {
                                        var e = compraRow.embList[currentIndex];
                                        if (e) {
                                            itensModel.setProperty(compraRow.index, "embId", e.id);
                                            itensModel.setProperty(compraRow.index, "fator", e.fator);
                                            var row = itensModel.get(compraRow.index);
                                            var est = App.itemEstoque(row.produtoId);
                                            if (est && est.custoMedio > 0)
                                                itensModel.setProperty(compraRow.index, "custoTexto",
                                                                       App.formatarValor(est.custoMedio * e.fator));
                                            novaCompraDialog.recomputar();
                                        }
                                    }
                                }
                                AppSpinBox {
                                    Layout.preferredWidth: 120
                                    from: 1; to: 100000; value: compraRow.qtd
                                    onValueModified: { itensModel.setProperty(compraRow.index, "qtd", value); novaCompraDialog.recomputar(); }
                                }
                                // Validade da remessa que está chegando. Fica aqui,
                                // e não no cadastro do produto, porque cada carga
                                // vence numa data — a nota é o momento em que se
                                // sabe qual é.
                                AppDateField {
                                    Layout.preferredWidth: 118
                                    text: compraRow.validadeTexto
                                    placeholderText: qsTr("validade")
                                    onTextChanged: itensModel.setProperty(compraRow.index,
                                                                          "validadeTexto", text)
                                }
                                AppTextField {
                                    Layout.preferredWidth: 112
                                    text: compraRow.custoTexto
                                    horizontalAlignment: Text.AlignRight
                                    placeholderText: qsTr("custo")
                                    onTextChanged: { itensModel.setProperty(compraRow.index, "custoTexto", text); novaCompraDialog.recomputar(); }
                                }
                                ToolButton { text: "✕"; onClicked: { itensModel.remove(compraRow.index); novaCompraDialog.recomputar(); } }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                ToggleButton { id: gerarConta; text: qsTr("Gerar conta a pagar") }
                Item { Layout.fillWidth: true }
                FormField {
                    label: qsTr("Vencimento")
                    visible: gerarConta.checked
                    Layout.preferredWidth: 150
                    AppDateField { id: vencField; width: parent.width }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Total da compra"); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text { text: App.formatarDinheiro(novaCompraDialog.totalCompra); color: Theme.primary; font.pixelSize: Theme.fontLg; font.weight: Font.Bold }
            }

            Label { id: erroCompra; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }

            RowLayout {
                Layout.fillWidth: true
                AppButton { kind: "accent"; text: qsTr("Registrar compra"); onClicked: novaCompraDialog.confirmar() }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: novaCompraDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ============ CADASTRO RÁPIDO DE PRODUTO (dentro da compra) ============
    AppDialog {
        id: novoProdutoRapido
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        function abrir(nome) {
            npNome.text = nome || "";
            npPreco.text = "";
            npUnidade.text = "unidade";
            npErro.text = "";
            open();
        }
        title: qsTr("Cadastrar produto")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: qsTr("Cadastro rápido só para já usar nesta compra. O custo você informa na própria compra; o restante (categoria, código de barras, embalagens) dá para completar depois em Produtos.")
                color: Theme.textMuted; font.pixelSize: Theme.fontSm
                Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
            FormField { label: qsTr("Nome *"); Layout.fillWidth: true; AppTextField { id: npNome; width: parent.width } }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                FormField {
                    label: qsTr("Preço de venda")
                    Layout.fillWidth: true
                    AppTextField { id: npPreco; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" }
                }
                FormField {
                    label: qsTr("Unidade")
                    Layout.preferredWidth: 130
                    AppTextField { id: npUnidade; width: parent.width; placeholderText: qsTr("unidade") }
                }
            }
            Label { id: npErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Cadastrar e adicionar")
                    onClicked: {
                        if (npNome.text.trim() === "") { npErro.text = qsTr("Informe o nome do produto."); return; }
                        var preco = App.parseDinheiro(npPreco.text);
                        if (preco < 0) preco = 0;
                        var un = npUnidade.text.trim() === "" ? "unidade" : npUnidade.text.trim();
                        var dados = {
                            id: 0, nome: npNome.text.trim(), categoriaId: 0,
                            unidadeBase: un, estoqueMinimo: 0, localizacao: "", composto: false,
                            embalagens: [{ id: 0, nome: un, fator: 1, codigoBarras: "", preco: preco }],
                            composicao: []
                        };
                        if (!App.salvarProduto(dados)) { npErro.text = App.ultimoErro(); return; }
                        var l = App.buscarProdutosPorNome(npNome.text.trim(), false);
                        if (l.length > 0) novaCompraDialog.adicionarProduto(l[0]);
                        buscaProduto.text = "";
                        sugCompra.clear();
                        novoProdutoRapido.close();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: novoProdutoRapido.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ======================= FORNECEDORES =======================
    AppDialog {
        id: fornecedoresDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 620
        padding: Theme.spacingLg
        property var atual: null

        onOpened: { App.recarregarFornecedores(); atual = null; }
        function editar(id) { atual = App.fornecedor(id); _preencher(); erroForn.text = ""; }
        function novo() { atual = App.novoFornecedor(); _preencher(); erroForn.text = ""; }
        function _preencher() {
            if (!atual) return;
            fNome.text = atual.nome || ""; fCnpj.text = atual.cnpj || "";
            fTel.text = atual.telefone || ""; fContato.text = atual.contato || "";
            fEmail.text = atual.email || ""; fEnd.text = atual.endereco || "";
        }
        function salvar() {
            var dados = { id: atual.id || 0, nome: fNome.text, cnpj: fCnpj.text,
                          telefone: fTel.text, contato: fContato.text, email: fEmail.text, endereco: fEnd.text };
            if (App.salvarFornecedor(dados)) { atual = null; }
            else erroForn.text = App.ultimoErro();
        }

        title: qsTr("Fornecedores")
        contentItem: RowLayout {
            spacing: Theme.spacingMd

            // Lista
            ColumnLayout {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                spacing: Theme.spacingSm
                AppButton { kind: "accent"; text: qsTr("＋ Novo"); Layout.fillWidth: true; onClicked: fornecedoresDialog.novo() }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 300
                    radius: Theme.radiusSm
                    color: Theme.surfaceAlt
                    border.color: Theme.border
                    clip: true
                    ListView {
                        anchors.fill: parent
                        clip: true
                        model: App.fornecedores
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            width: ListView.view.width
                            required property int idFornecedor
                            required property string nome
                            required property string telefone
                            contentItem: ColumnLayout {
                                spacing: 0
                                Text { text: nome; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: telefone; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            onClicked: fornecedoresDialog.editar(idFornecedor)
                        }
                    }
                }
            }

            // Form
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                visible: fornecedoresDialog.atual !== null
                FormField { label: qsTr("Nome *"); Layout.fillWidth: true; AppTextField { id: fNome; width: parent.width } }
                RowLayout {
                    Layout.fillWidth: true
                    FormField { label: qsTr("CNPJ"); Layout.fillWidth: true; AppTextField { id: fCnpj; width: parent.width } }
                    FormField { label: qsTr("Telefone"); Layout.fillWidth: true; AppTextField { id: fTel; width: parent.width } }
                }
                FormField { label: qsTr("Contato"); Layout.fillWidth: true; AppTextField { id: fContato; width: parent.width } }
                FormField { label: qsTr("E-mail"); Layout.fillWidth: true; AppTextField { id: fEmail; width: parent.width } }
                FormField { label: qsTr("Endereço"); Layout.fillWidth: true; AppTextField { id: fEnd; width: parent.width } }
                Label { id: erroForn; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                RowLayout {
                    Layout.fillWidth: true
                    AppButton { kind: "accent"; text: qsTr("Salvar"); onClicked: fornecedoresDialog.salvar() }
                    AppButton { kind: "default"; text: qsTr("Fechar"); onClicked: fornecedoresDialog.close() }
                    Item { Layout.fillWidth: true }
                }
            }
            Label {
                visible: fornecedoresDialog.atual === null
                Layout.fillWidth: true
                text: qsTr("Selecione um fornecedor ou clique em “Novo”.")
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }
    }
}
