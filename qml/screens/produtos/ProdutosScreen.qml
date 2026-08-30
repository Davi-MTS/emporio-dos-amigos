import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Tela de Produtos: lista à esquerda, editor à direita.
// Prático: busca por nome OU código de barras, produto novo já vem com a
// embalagem base, poucos campos obrigatórios (só nome).
Rectangle {
    id: tela
    color: Theme.background

    // Produto em edição (QVariantMap vindo do backend) ou null.
    property var produtoAtual: null
    property var listaCategorias: App.categorias()
    property var origensDose: []

    function recarregarOrigens() {
        origensDose = App.produtosParaOrigemDose(
            tela.produtoAtual ? (tela.produtoAtual.id || 0) : 0);
    }

    // Diz, com os números do próprio produto, o que a dose vai fazer.
    function explicarDose() {
        var origem = null;
        for (var i = 0; i < origensDose.length; i++) {
            if (origensDose[i].id === doseOrigemCombo.currentValue) {
                origem = origensDose[i];
                break;
            }
        }
        var qtd = parseInt(doseQtdField.text) || 0;
        if (!origem || qtd <= 0)
            return qsTr("Escolha o produto de origem e quanto cada venda consome dele.");
        var cabem = Math.floor(origem.estoque / qtd);
        return qsTr("Cada venda tira ") + qtd + " " + origem.unidadeBase
               + qsTr(" de ") + origem.nome + qsTr(". Com ") + origem.estoque + " "
               + origem.unidadeBase + qsTr(" em estoque, dá para ") + cabem
               + qsTr(" vendas. Este produto não terá estoque próprio.");
    }
    readonly property var unidades: ["unidade", "ml", "litro", "g", "kg"]
    readonly property bool podeEditar: {
        var p = (App.usuarioAtual && App.usuarioAtual.permissoes) ? App.usuarioAtual.permissoes : ({});
        return p.tudo === true || p.edita_produto === true;
    }

    // Larguras de coluna compartilhadas entre cabeçalho e linhas.
    // Colunas de largura fixa somem conforme a janela encolhe. Sem isto elas não
    // cabem na lista e passam por cima do nome do produto (visto com a janela
    // restaurada, fora de tela cheia).
    readonly property bool mostrarEstoque:   listaBox.width > 250
    readonly property bool mostrarStatus:    listaBox.width > 350
    readonly property bool mostrarPreco:     listaBox.width > 460
    readonly property bool mostrarCategoria: listaBox.width > 600

    readonly property int colCat: 130
    readonly property int colEst: 84
    readonly property int colPreco: 96
    readonly property int colStatus: 92

    ListModel { id: embModel }
    ListModel { id: compModel }   // composição do copão (insumoId, insumoNome, quantidade)

    // Recarrega a lista de categorias e deixa selecionada a que acabou de nascer.
    function recarregarCategorias(selecionarId) {
        listaCategorias = App.categorias();
        if (selecionarId > 0)
            categoriaCombo.currentIndex = categoriaCombo.indexOfValue(selecionarId);
    }

    // Explica, em português e com exemplo, o que a unidade base significa na
    // hora de dar entrada no estoque.
    function explicarUnidade(u) {
        if (u === "ml")
            return qsTr("O estoque deste produto é contado em ML. Uma garrafa de 1 litro entra como 1000. Use quando for vender em dose.");
        if (u === "litro")
            return qsTr("O estoque é contado em LITROS. Meio litro não existe aqui — para vender dose, use ml.");
        if (u === "g")
            return qsTr("O estoque é contado em GRAMAS. Um pacote de 500 g entra como 500.");
        if (u === "kg")
            return qsTr("O estoque é contado em QUILOS. Para vender por grama, use g.");
        if (u === "unidade")
            return qsTr("O estoque é contado em UNIDADES (garrafas, latas, pacotes). É o caso da maioria dos produtos.");
        return qsTr("O estoque deste produto é contado em \"") + u + qsTr("\".");
    }

    function abrirNovo() {
        produtoAtual = App.novoProduto();
        _carregarEmbalagens();
        _carregarComposicao();
        _preencherCampos();
        erroLabel.text = "";
    }
    function abrirProduto(id) {
        produtoAtual = App.produto(id);
        _carregarEmbalagens();
        _carregarComposicao();
        _preencherCampos();
        erroLabel.text = "";
    }
    function _carregarComposicao() {
        compModel.clear();
        if (!produtoAtual) return;
        var lista = produtoAtual.composicao || [];
        for (var i = 0; i < lista.length; i++) {
            var c = lista[i];
            compModel.append({ categoriaId: c.categoriaId || 0,
                               unidade: c.unidade || "unidade",
                               quantidade: c.quantidade || 1 });
        }
    }
    function fecharEditor() {
        produtoAtual = null;
    }
    // Preenche os controles imperativamente (evita o problema de binding
    // "quebrado" ao editar e reabrir outro produto).
    function _preencherCampos() {
        if (!produtoAtual)
            return;
        nomeField.text = produtoAtual.nome || "";
        unidadeField.text = produtoAtual.unidadeBase || "unidade";
        minimoSpin.value = produtoAtual.estoqueMinimo || 0;
        localField.text = produtoAtual.localizacao || "";
        categoriaCombo.currentIndex = categoriaCombo.indexOfValue(produtoAtual.categoriaId || 0);
        compostoCheck.checked = produtoAtual.composto || false;
        doseCheck.checked = (produtoAtual.doseDeProdutoId || 0) > 0;
        doseQtdField.text = (produtoAtual.doseQuantidade || 0) > 0
                            ? ("" + produtoAtual.doseQuantidade) : "";
        recarregarOrigens();
        if (doseCheck.checked)
            doseOrigemCombo.currentIndex = doseOrigemCombo.indexOfValue(produtoAtual.doseDeProdutoId);
        abasProduto.currentIndex = 0;   // sempre abre em "Dados"
    }
    function _carregarEmbalagens() {
        embModel.clear();
        if (!produtoAtual)
            return;
        var lista = produtoAtual.embalagens || [];
        for (var i = 0; i < lista.length; i++) {
            var e = lista[i];
            embModel.append({
                embId: e.id || 0,
                nome: e.nome || "",
                fator: e.fator || 1,
                codigoBarras: e.codigoBarras || "",
                precoTexto: App.formatarValor(e.preco || 0)
            });
        }
    }
    function salvar() {
        var dados = {
            id: produtoAtual.id || 0,
            nome: nomeField.text,
            categoriaId: categoriaCombo.currentValue ? categoriaCombo.currentValue : 0,
            unidadeBase: unidadeField.text,
            estoqueMinimo: minimoSpin.value,
            localizacao: localField.text,
            composto: compostoCheck.checked && !doseCheck.checked,
            doseDeProdutoId: doseCheck.checked ? (doseOrigemCombo.currentValue || 0) : 0,
            doseQuantidade: doseCheck.checked ? (parseInt(doseQtdField.text) || 0) : 0,
            embalagens: [],
            composicao: []
        };
        if (compostoCheck.checked) {
            for (var c = 0; c < compModel.count; c++) {
                var ci = compModel.get(c);
                if (ci.categoriaId > 0)
                    dados.composicao.push({ categoriaId: ci.categoriaId, unidade: ci.unidade, quantidade: ci.quantidade });
            }
        }
        for (var i = 0; i < embModel.count; i++) {
            var e = embModel.get(i);
            var cents = App.parseDinheiro(e.precoTexto);
            dados.embalagens.push({
                id: e.embId,
                nome: e.nome,
                fator: e.fator,
                codigoBarras: e.codigoBarras,
                preco: cents < 0 ? 0 : cents
            });
        }
        if (App.salvarProduto(dados))
            fecharEditor();
        else
            erroLabel.text = App.ultimoErro();
    }

    // ---------------------------------------------------------------- layout
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // ============================== LISTA ==============================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                AppTextField {
                    id: buscaField
                    Layout.fillWidth: true
                    Layout.maximumWidth: 420
                    placeholderText: qsTr("Buscar por nome ou código de barras…")
                    onTextChanged: App.recarregarProdutos(text)
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    kind: "accent"
                    text: qsTr("＋ Novo produto")
                    enabled: tela.podeEditar
                    onClicked: tela.abrirNovo()
                }
            }

            Rectangle {
                id: listaBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 260
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
                            Text { text: qsTr("Categoria"); visible: tela.mostrarCategoria; Layout.preferredWidth: tela.colCat; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                            Text { text: qsTr("Estoque"); visible: tela.mostrarEstoque; Layout.preferredWidth: tela.colEst; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                            Text { text: qsTr("Preço"); visible: tela.mostrarPreco; Layout.preferredWidth: tela.colPreco; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                            Text { text: qsTr("Status"); visible: tela.mostrarStatus; Layout.preferredWidth: tela.colStatus; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                    ListView {
                        id: lista
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: App.produtos
                        ScrollBar.vertical: ScrollBar {}

                        delegate: ItemDelegate {
                            id: linha
                            required property int index
                            required property int idProduto
                            required property string nome
                            required property string categoria
                            required property var estoque
                            required property var preco
                            required property string status
                            required property bool composto

                            width: ListView.view.width
                            height: 46
                            leftPadding: Theme.spacingMd
                            rightPadding: Theme.spacingMd
                            highlighted: tela.produtoAtual && tela.produtoAtual.id === idProduto
                            onClicked: tela.abrirProduto(idProduto)

                            contentItem: RowLayout {
                                spacing: Theme.spacingSm
                                Text {
                                    Layout.fillWidth: true
                                    text: linha.nome
                                    elide: Text.ElideRight
                                    color: Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                }
                                Text { visible: tela.mostrarCategoria; Layout.preferredWidth: tela.colCat; text: linha.categoria; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                                Text { visible: tela.mostrarEstoque; Layout.preferredWidth: tela.colEst; text: linha.composto ? "—" : linha.estoque; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                                Text { visible: tela.mostrarPreco; Layout.preferredWidth: tela.colPreco; text: App.formatarDinheiro(linha.preco); horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                                Item {
                                    visible: tela.mostrarStatus
                                    Layout.preferredWidth: tela.colStatus
                                    implicitHeight: 22
                                    StatusBadge { visible: !linha.composto; status: linha.status; anchors.verticalCenter: parent.verticalCenter }
                                    Rectangle {
                                        visible: linha.composto
                                        anchors.verticalCenter: parent.verticalCenter
                                        implicitWidth: cTxt.implicitWidth + 16; implicitHeight: 22; radius: 6
                                        color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.15)
                                        Text { id: cTxt; anchors.centerIn: parent; text: qsTr("Composto"); color: Theme.primary; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                    }
                                }
                            }
                        }

                        // Estado vazio
                        Label {
                            anchors.centerIn: parent
                            visible: lista.count === 0
                            text: buscaField.text.length > 0
                                  ? qsTr("Nenhum produto encontrado.")
                                  : qsTr("Nenhum produto cadastrado.\nClique em “Novo produto”.")
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.textMuted
                        }
                    }
                }
            }
        }

        // ============================== EDITOR =============================
        Rectangle {
            Layout.preferredWidth: 520
            Layout.minimumWidth: 380
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            // Placeholder quando nada está em edição
            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingLg
                visible: tela.produtoAtual === null
                text: qsTr("Selecione um produto na lista para editar,\nou clique em “Novo produto”.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textMuted
            }

            ScrollView {
                id: editorScroll
                anchors.fill: parent
                visible: tela.produtoAtual !== null
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: editorScroll.availableWidth
                    spacing: Theme.spacingMd

                    // Cabeçalho do editor
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.spacingLg
                        Layout.bottomMargin: 0
                        spacing: 2
                        Text {
                            text: (tela.produtoAtual && tela.produtoAtual.id > 0)
                                  ? qsTr("Editando produto") : qsTr("Novo produto")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingLg
                        Layout.rightMargin: Theme.spacingLg
                        spacing: Theme.spacingMd

                        // O editor é dividido em seções para não virar um
                        // formulário gigante empilhado.
                        SegmentedControl {
                            id: abasProduto
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            Layout.bottomMargin: Theme.spacingXs
                            options: [qsTr("Dados"), qsTr("Embalagens"), qsTr("Dose / Receita")]
                        }

                        // ========================= SEÇÃO: DADOS =========================
                        ColumnLayout {
                        visible: abasProduto.currentIndex === 0
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        FormField {
                            label: qsTr("Nome *")
                            Layout.fillWidth: true
                            AppTextField {
                                id: nomeField
                                width: parent.width
                                placeholderText: qsTr("Ex.: Heineken Long Neck 330ml")
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField {
                                label: qsTr("Categoria")
                                Layout.fillWidth: true
                                RowLayout {
                                    width: parent.width
                                    spacing: Theme.spacingSm
                                    AppComboBox {
                                        id: categoriaCombo
                                        Layout.fillWidth: true
                                        model: tela.listaCategorias
                                        textRole: "nome"
                                        valueRole: "id"
                                    }
                                    // Antes só dava para usar as 12 categorias do seed: se
                                    // chegasse um produto que não se encaixa, o cadastro
                                    // parava até alguém mexer no banco.
                                    AppButton {
                                        kind: "ghost"
                                        text: "＋"
                                        implicitWidth: 40
                                        enabled: tela.podeEditar
                                        onClicked: novaCategoriaDialog.abrir()
                                    }
                                }
                            }
                            FormField {
                                label: qsTr("Unidade base")
                                Layout.preferredWidth: 150
                                AppTextField {
                                    id: unidadeField
                                    width: parent.width
                                    text: tela.produtoAtual ? (tela.produtoAtual.unidadeBase || "unidade") : "unidade"
                                }
                            }
                        }

                        // Atalhos de unidade base (o campo aceita qualquer outra digitada).
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: qsTr("Unidade:"); color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            Repeater {
                                model: ["unidade", "ml", "litro", "g", "kg"]
                                delegate: Rectangle {
                                    required property string modelData
                                    radius: 6
                                    implicitHeight: 24
                                    implicitWidth: chipTxt.implicitWidth + 16
                                    color: unidadeField.text === modelData ? Theme.accentSoft : Theme.surfaceAlt
                                    border.color: unidadeField.text === modelData ? Theme.primary : Theme.border
                                    Text {
                                        id: chipTxt
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: unidadeField.text === modelData ? Theme.primary : Theme.textMuted
                                        font.pixelSize: Theme.fontSm
                                        font.weight: Font.DemiBold
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: unidadeField.text = modelData }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        // A unidade base decide como o estoque é CONTADO, e era
                        // isso que confundia na hora de dar entrada. Aqui a
                        // consequência aparece escrita, com exemplo.
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: tela.explicarUnidade(unidadeField.text)
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField {
                                label: qsTr("Estoque mínimo")
                                Layout.preferredWidth: 150
                                AppSpinBox {
                                    id: minimoSpin
                                    width: parent.width
                                    from: 0; to: 1000000
                                    value: tela.produtoAtual ? (tela.produtoAtual.estoqueMinimo || 0) : 0
                                }
                            }
                            FormField {
                                label: qsTr("Localização")
                                Layout.fillWidth: true
                                AppTextField {
                                    id: localField
                                    width: parent.width
                                    text: tela.produtoAtual ? (tela.produtoAtual.localizacao || "") : ""
                                    placeholderText: qsTr("Ex.: Câmara fria A")
                                }
                            }
                        }

                        } // fim SEÇÃO: DADOS

                        // ====================== SEÇÃO: EMBALAGENS ======================
                        ColumnLayout {
                        visible: abasProduto.currentIndex === 1
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        Text {
                            text: qsTr("Fator = quantos \"") + unidadeField.text
                                  + qsTr("\" cabem nesta embalagem. Ex.: fardo de 12 latas = 12; garrafa de 1 litro contada em ml = 1000.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        // Cabeçalho das embalagens
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            Text { text: qsTr("Nome"); Layout.preferredWidth: 130; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { text: qsTr("Fator (") + unidadeField.text + ")"; Layout.preferredWidth: 96; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { text: qsTr("Cód. barras"); Layout.fillWidth: true; Layout.minimumWidth: 90; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { text: qsTr("Preço"); Layout.preferredWidth: 104; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Item { Layout.preferredWidth: 28 }
                        }

                        Repeater {
                            model: embModel
                            delegate: RowLayout {
                                required property int index
                                required property string nome
                                required property int fator
                                required property string codigoBarras
                                required property string precoTexto
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm

                                AppTextField {
                                    Layout.preferredWidth: 130
                                    text: nome
                                    onTextChanged: embModel.setProperty(index, "nome", text)
                                }
                                AppSpinBox {
                                    Layout.preferredWidth: 96
                                    from: 1; to: 100000
                                    value: fator
                                    onValueModified: embModel.setProperty(index, "fator", value)
                                }
                                AppTextField {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 90
                                    text: codigoBarras
                                    placeholderText: qsTr("bipar…")
                                    onTextChanged: embModel.setProperty(index, "codigoBarras", text)
                                }
                                AppTextField {
                                    Layout.preferredWidth: 104
                                    text: precoTexto
                                    horizontalAlignment: Text.AlignRight
                                    onTextChanged: embModel.setProperty(index, "precoTexto", text)
                                }
                                ToolButton {
                                    Layout.preferredWidth: 28
                                    text: "✕"
                                    enabled: embModel.count > 1
                                    onClicked: embModel.remove(index)
                                }
                            }
                        }

                        AppButton {
                            kind: "ghost"
                            text: qsTr("＋ Adicionar embalagem")
                            onClicked: embModel.append({
                                embId: 0, nome: "", fator: 1, codigoBarras: "", precoTexto: "0,00"
                            })
                        }

                        } // fim SEÇÃO: EMBALAGENS

                        // ====================== SEÇÃO: COMPOSIÇÃO ======================
                        // Produto composto (copão, drink, shot, dose).
                        ColumnLayout {
                        visible: abasProduto.currentIndex === 2
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        // ---- Dose: o caso simples e o mais comum ----
                        // Vender dose pelo produto COMPOSTO obrigava a escolher a
                        // bebida em cada venda, com diálogo. Para "dose de whisky"
                        // a garrafa é sempre a mesma: aqui ela é apontada uma vez.
                        ToggleButton {
                            id: doseCheck
                            text: qsTr("É uma dose tirada de outro produto")
                            onCheckedChanged: {
                                if (checked) {
                                    compostoCheck.checked = false;
                                    tela.recarregarOrigens();
                                }
                            }
                        }
                        Text {
                            visible: !doseCheck.checked && !compostoCheck.checked
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Ative se este produto sai de dentro de outro — a dose que sai da garrafa, o copo tirado do barril. Ele vende com um bipe, mas quem baixa do estoque é o produto de origem.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                        RowLayout {
                            visible: doseCheck.checked
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            FormField {
                                label: qsTr("Sai de qual produto")
                                Layout.fillWidth: true
                                AppComboBox {
                                    id: doseOrigemCombo
                                    width: parent.width
                                    model: tela.origensDose
                                    textRole: "nome"
                                    valueRole: "id"
                                }
                            }
                            FormField {
                                label: qsTr("Consome quanto")
                                Layout.preferredWidth: 130
                                AppTextField {
                                    id: doseQtdField
                                    width: parent.width
                                    horizontalAlignment: Text.AlignRight
                                    placeholderText: "50"
                                    inputMethodHints: Qt.ImhDigitsOnly
                                }
                            }
                        }
                        Text {
                            visible: doseCheck.checked
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: tela.explicarDose()
                            color: Theme.text
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            visible: !doseCheck.checked
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.border
                        }

                        ToggleButton {
                            id: compostoCheck
                            visible: !doseCheck.checked
                            text: qsTr("Produto composto (copão, drink, shot…)")
                            onCheckedChanged: if (checked) doseCheck.checked = false
                        }
                        Text {
                            visible: !compostoCheck.checked && !doseCheck.checked
                            text: qsTr("Ative se este produto for montado na hora a partir de outros (ex.: copão). "
                                       + "Produtos normais não precisam de composição.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Text {
                            visible: compostoCheck.checked
                            text: qsTr("A receita é por CATEGORIA (ex.: Destilados em ml, Gelo em unidade). "
                                       + "Na venda, você escolhe qual produto de cada categoria (qual bebida, qual gelo). "
                                       + "O composto não tem estoque próprio.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            visible: compostoCheck.checked
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            Text { text: qsTr("Categoria"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { text: qsTr("Qtd"); Layout.preferredWidth: 110; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Text { text: qsTr("Unidade"); Layout.preferredWidth: 120; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                            Item { Layout.preferredWidth: 28 }
                        }
                        Repeater {
                            model: compModel
                            delegate: RowLayout {
                                id: compRow
                                required property int index
                                required property int categoriaId
                                required property int quantidade
                                required property string unidade
                                visible: compostoCheck.checked
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                AppComboBox {
                                    Layout.fillWidth: true
                                    model: tela.listaCategorias
                                    textRole: "nome"
                                    valueRole: "id"
                                    Component.onCompleted: currentIndex = indexOfValue(compRow.categoriaId)
                                    // 'index' aqui é o parâmetro do sinal (item do combo); use compRow.index.
                                    onActivated: compModel.setProperty(compRow.index, "categoriaId", currentValue)
                                }
                                AppSpinBox {
                                    Layout.preferredWidth: 110
                                    from: 1; to: 1000000; value: compRow.quantidade
                                    onValueModified: compModel.setProperty(compRow.index, "quantidade", value)
                                }
                                AppComboBox {
                                    Layout.preferredWidth: 120
                                    model: tela.unidades
                                    Component.onCompleted: currentIndex = tela.unidades.indexOf(compRow.unidade)
                                    onActivated: compModel.setProperty(compRow.index, "unidade", currentText)
                                }
                                ToolButton { text: "✕"; onClicked: compModel.remove(compRow.index) }
                            }
                        }
                        AppButton {
                            visible: compostoCheck.checked
                            kind: "ghost"
                            text: qsTr("＋ Adicionar categoria")
                            onClicked: compModel.append({ categoriaId: 0, unidade: "unidade", quantidade: 1 })
                        }

                        } // fim SEÇÃO: COMPOSIÇÃO

                        Label {
                            id: erroLabel
                            Layout.fillWidth: true
                            visible: text.length > 0
                            color: Theme.danger
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSm
                        }
                    }

                    // Ações
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.spacingLg
                        spacing: Theme.spacingSm
                        AppButton {
                            kind: "accent"
                            text: qsTr("Salvar")
                            enabled: tela.podeEditar
                            onClicked: tela.salvar()
                        }
                        AppButton {
                            kind: "default"
                            text: qsTr("Cancelar")
                            onClicked: tela.fecharEditor()
                        }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            kind: "default"
                            text: qsTr("Excluir")
                            visible: tela.produtoAtual && tela.produtoAtual.id > 0 && tela.podeEditar
                            onClicked: confirmarExclusao.open()
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: confirmarExclusao
        anchors.centerIn: parent
        modal: true
        title: qsTr("Excluir produto")
        standardButtons: Dialog.Yes | Dialog.No
        Label { text: qsTr("O produto será desativado (não some do histórico). Confirmar?") }
        onAccepted: {
            if (tela.produtoAtual)
                App.inativarProduto(tela.produtoAtual.id);
            tela.fecharEditor();
        }
    }

    // ------------------------------------------------------- Nova categoria
    AppDialog {
        id: novaCategoriaDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Nova categoria")

        function abrir() {
            nomeCategoriaField.text = "";
            erroCategoria.text = "";
            open();
            nomeCategoriaField.forceActiveFocus();
        }

        function salvar() {
            var id = App.criarCategoria(nomeCategoriaField.text);
            if (id > 0) {
                tela.recarregarCategorias(id);
                novaCategoriaDialog.close();
            } else {
                erroCategoria.text = App.ultimoErro();
            }
        }

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingSm

            FormField {
                label: qsTr("Nome da categoria")
                Layout.fillWidth: true
                AppTextField {
                    id: nomeCategoriaField
                    width: parent.width
                    placeholderText: qsTr("Ex.: Doces, Salgadinhos, Sorvete…")
                    onAccepted: novaCategoriaDialog.salvar()
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Se já existir uma categoria com esse nome, ela é reaproveitada.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
            Label {
                id: erroCategoria
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: text.length > 0
                color: Theme.danger
                font.pixelSize: Theme.fontSm
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingSm
                AppButton {
                    kind: "accent"
                    text: qsTr("Criar")
                    onClicked: novaCategoriaDialog.salvar()
                }
                AppButton {
                    kind: "default"
                    text: qsTr("Cancelar")
                    onClicked: novaCategoriaDialog.close()
                }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
