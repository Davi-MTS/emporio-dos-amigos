import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// PDV — frente de caixa. Keyboard-first: bipe o código (Enter adiciona), ou
// digite o nome (sugestões). Múltiplos pagamentos com troco. Ao finalizar,
// baixa o estoque. Requer um caixa aberto.
Rectangle {
    id: tela
    color: Theme.background

    // Carrinho e pagamentos.
    ListModel { id: cart }        // produtoId, nome, embId, embNome, fator, preco, qtd, desconto
    ListModel { id: pagsModel }   // forma, valor
    ListModel { id: sugestoes }   // itens de busca por nome

    readonly property bool podeDarDesconto: {
        var p = (App.usuarioAtual && App.usuarioAtual.permissoes) ? App.usuarioAtual.permissoes : ({});
        return p.tudo === true || p.pode_dar_desconto === true;
    }

    property int clienteId: 0
    property string clienteNome: qsTr("Consumidor final")
    property int descontoGeral: 0
    property int totalVenda: 0
    property int pagoVenda: 0
    readonly property int faltaVenda: Math.max(0, totalVenda - pagoVenda)
    readonly property int trocoVenda: Math.max(0, pagoVenda - totalVenda)
    // Aviso (não bloqueante) quando o carrinho pede mais do que há em estoque.
    property string avisoEstoque: ""
    // Com a janela restaurada (fora de tela cheia) as colunas fixas do carrinho
    // não cabem e se sobrepõem: escondemos as menos essenciais primeiro.
    property real larguraCarrinho: 0
    readonly property bool mostrarPrecoUnit: larguraCarrinho > 470
    readonly property bool mostrarSubtotal:  larguraCarrinho > 360

    // Linha rótulo/valor (resumo do fechamento).
    component KV: RowLayout {
        id: kvRoot
        property string k: ""
        property string v: ""
        property color cor: Theme.textMuted
        Layout.fillWidth: true
        Text { text: kvRoot.k; Layout.fillWidth: true; color: kvRoot.cor; font.pixelSize: Theme.fontSm }
        Text { text: kvRoot.v; color: kvRoot.cor; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
    }

    // Leitura do carrinho de fora da tela (usado pelos testes de interface, que
    // não enxergam o ListModel interno). Devolve null se a linha não existe.
    function linhaCarrinho(i) { return (i >= 0 && i < cart.count) ? cart.get(i) : null; }
    function itensNoCarrinho() { return cart.count; }

    function recomputar() {
        var s = 0;
        for (var i = 0; i < cart.count; i++) {
            var e = cart.get(i);
            s += e.qtd * e.preco - e.desconto;
        }
        totalVenda = Math.max(0, s - descontoGeral);
        var p = 0;
        for (var j = 0; j < pagsModel.count; j++)
            p += pagsModel.get(j).valor;
        pagoVenda = p;
        _conferirEstoque();
    }

    // Soma a necessidade de cada produto NORMAL no carrinho e compara com o
    // estoque atual. Não bloqueia a venda — só avisa (o comerciante decide).
    // Produtos compostos (copão, drink…) são feitos na hora e NÃO exigem estoque:
    // ficam de fora do aviso, tanto o composto quanto os insumos que ele consome.
    function _conferirEstoque() {
        var need = {};   // produtoId -> unidades base necessárias
        var nomes = {};  // produtoId -> nome para exibir
        for (var i = 0; i < cart.count; i++) {
            var e = cart.get(i);
            // Linha de composto (tem insumos): ignora — não precisa de estoque.
            if (e.insumosJson && e.insumosJson !== "[]")
                continue;
            var base = e.qtd * (e.fator > 0 ? e.fator : 1);
            need[e.produtoId] = (need[e.produtoId] || 0) + base;
            nomes[e.produtoId] = e.nome;
        }
        var avisos = [];
        for (var pid in need) {
            var disp = App.estoqueDisponivel(parseInt(pid));
            if (need[pid] > disp)
                avisos.push((nomes[pid] || ("#" + pid)) + qsTr(" (tem ") + disp
                            + qsTr(", precisa ") + need[pid] + ")");
        }
        avisoEstoque = avisos.length > 0 ? (qsTr("Estoque insuficiente: ") + avisos.join("; ")) : "";
    }

    function adicionar(item) {
        if (item.composto === true) { _resolverComposto(item); return; }
        // Produto simples: agrupa se já estiver no carrinho.
        for (var i = 0; i < cart.count; i++) {
            var e = cart.get(i);
            if (e.produtoId === item.produtoId && e.embId === item.embalagemId
                    && (!e.insumosJson || e.insumosJson === "[]")) {
                cart.setProperty(i, "qtd", e.qtd + 1);
                recomputar();
                return;
            }
        }
        // insumosJson: lista de insumos serializada (o ListModel não preserva
        // arrays de objetos de forma confiável; a string atravessa sem perdas).
        cart.append({
            produtoId: item.produtoId, nome: item.nome,
            embId: item.embalagemId, embNome: item.embalagemNome,
            fator: item.fator, preco: item.preco, qtd: 1, desconto: 0,
            insumosJson: "[]", insumosLabel: "",
            // Todas as embalagens do produto (unidade/caixa/fardo) para trocar na linha.
            embalagensJson: JSON.stringify(App.embalagensDe(item.produtoId))
        });
        recomputar();
    }

    function _resolverComposto(item) {
        var linhas = App.composicaoParaVenda(item.produtoId);
        if (linhas.length === 0) { avisoScan.mostrar(qsTr("Composto sem receita cadastrada.")); return; }
        // Sempre abre a tela para escolher/confirmar os insumos exatos.
        compostoDialog.abrir(item, linhas);
    }

    function _adicionarComposto(item, escolhas, preco) {
        var insumos = [];
        var nomes = [];
        for (var i = 0; i < escolhas.length; i++) {
            insumos.push({ produtoId: escolhas[i].produtoId, quantidade: escolhas[i].quantidade, nome: escolhas[i].nome });
            nomes.push(escolhas[i].nome);
        }
        var precoFinal = (preco !== undefined && preco >= 0) ? preco : item.preco;
        cart.append({
            produtoId: item.produtoId, nome: item.nome,
            embId: item.embalagemId, embNome: item.embalagemNome,
            fator: item.fator, preco: precoFinal, qtd: 1, desconto: 0,
            insumosJson: JSON.stringify(insumos), insumosLabel: nomes.join(", "),
            embalagensJson: "[]"   // composto não troca embalagem
        });
        recomputar();
        scanField.forceActiveFocus();
    }

    function atualizarSugestoes(texto) {
        sugestoes.clear();
        if (texto.trim().length < 2)
            return;
        var lista = App.buscarProdutosPorNome(texto);
        for (var i = 0; i < lista.length; i++)
            sugestoes.append(lista[i]);
    }

    function processarEntrada() {
        var texto = scanField.text.trim();
        if (texto.length === 0)
            return;
        var r = App.buscarProdutoPorCodigo(texto);
        if (r.encontrado) {
            adicionar(r);
            scanField.clear();
            sugestoes.clear();
            return;
        }
        if (sugestoes.count > 0) {
            adicionar(sugestoes.get(0));
            scanField.clear();
            sugestoes.clear();
            return;
        }
        avisoScan.mostrar(qsTr("Produto não encontrado."));
    }

    function addPagamento(forma) {
        var val = scanValor();
        if (val <= 0)
            return;
        // Só DINHEIRO pode exceder o total (a diferença volta como troco).
        // Pix/cartão/fiado não geram troco: limita ao que ainda falta, senão a
        // venda registraria um "troco" de dinheiro que nunca entrou na gaveta.
        if (forma !== "dinheiro" && val > faltaVenda) {
            if (faltaVenda <= 0) {
                avisoScan.mostrar(qsTr("Venda já está paga."));
                return;
            }
            val = faltaVenda;
            avisoScan.mostrar(qsTr("Ajustado para o valor que faltava."));
        }
        pagsModel.append({ forma: forma, valor: val });
        valorField.clear();
        recomputar();
    }
    function scanValor() {
        var t = valorField.text.trim();
        if (t.length === 0)
            return faltaVenda;
        var v = App.parseDinheiro(t);
        return v < 0 ? 0 : v;
    }

    function limparVenda() {
        cart.clear();
        pagsModel.clear();
        sugestoes.clear();
        descontoGeral = 0;
        descontoField.text = "";
        valorField.text = "";
        clienteId = 0;
        clienteNome = qsTr("Consumidor final");
        recomputar();
        scanField.forceActiveFocus();
    }

    function finalizar() {
        if (cart.count === 0) {
            avisoScan.mostrar(qsTr("Carrinho vazio."));
            return;
        }
        if (pagoVenda < totalVenda) {
            avisoScan.mostrar(qsTr("Falta ") + App.formatarDinheiro(faltaVenda));
            return;
        }
        var itens = [];
        for (var i = 0; i < cart.count; i++) {
            var e = cart.get(i);
            var ins = e.insumosJson ? JSON.parse(e.insumosJson) : [];
            itens.push({ produtoId: e.produtoId, embalagemId: e.embId, fator: e.fator,
                         qtd: e.qtd, precoUnit: e.preco, desconto: e.desconto,
                         insumos: ins });
        }
        var pags = [];
        for (var j = 0; j < pagsModel.count; j++) {
            var pg = pagsModel.get(j);
            pags.push({ forma: pg.forma, valor: pg.valor });
        }
        var res = App.finalizarVenda({ desconto: descontoGeral, clienteId: clienteId,
                                       itens: itens, pagamentos: pags });
        if (res.ok) {
            sucessoDialog.troco = res.troco;
            sucessoDialog.open();
            limparVenda();
        } else {
            avisoScan.mostrar(res.erro);
        }
    }

    // Atalhos de teclado.
    Shortcut { sequence: "F12"; onActivated: tela.finalizar() }
    Shortcut { sequence: "Esc"; onActivated: tela.limparVenda() }
    Shortcut { sequence: "F4"; enabled: tela.podeDarDesconto; onActivated: descontoField.forceActiveFocus() }
    Shortcut { sequence: "F2"; onActivated: scanField.forceActiveFocus() }
    Shortcut { sequence: "F8"; onActivated: clienteDialog.abrir() }

    // ============================ CAIXA FECHADO ============================
    Item {
        anchors.fill: parent
        visible: !App.caixaAberto

        Rectangle {
            anchors.centerIn: parent
            width: 380
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: colAbrir.implicitHeight + 2 * Theme.spacingLg

            ColumnLayout {
                id: colAbrir
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingLg
                spacing: Theme.spacingMd

                Text {
                    text: qsTr("Abrir caixa")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Informe o troco inicial para começar a vender.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                FormField {
                    label: qsTr("Troco inicial")
                    Layout.fillWidth: true
                    AppTextField {
                        id: aberturaField
                        width: parent.width
                        placeholderText: qsTr("ex.: 100,00")
                        horizontalAlignment: Text.AlignRight
                        onAccepted: App.abrirCaixa(text)
                    }
                }
                AppButton {
                    kind: "accent"
                    text: qsTr("Abrir caixa")
                    Layout.fillWidth: true
                    onClicked: App.abrirCaixa(aberturaField.text)
                }
            }
        }
    }

    // ============================ CAIXA ABERTO ============================
    ColumnLayout {
        anchors.fill: parent
        visible: App.caixaAberto
        spacing: 0

        // Barra do caixa
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 46
            color: Theme.surface
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLg
                anchors.rightMargin: Theme.spacingLg
                spacing: Theme.spacingSm
                Text { text: qsTr("Caixa aberto"); color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                Item { Layout.fillWidth: true }
                AppButton { kind: "ghost"; text: qsTr("Sangria"); onClicked: movCaixaDialog.abrir("sangria") }
                AppButton { kind: "ghost"; text: qsTr("Suprimento"); onClicked: movCaixaDialog.abrir("suprimento") }
                AppButton { kind: "default"; text: qsTr("Fechar caixa"); onClicked: fecharDialog.abrir() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spacingLg
            spacing: Theme.spacingMd

            // -------------------- Esquerda: scan + carrinho --------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingMd

                // Campo de scan
                Rectangle {
                    Layout.fillWidth: true
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.primary
                    border.width: 2
                    implicitHeight: 60

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingMd

                        AppIcon { name: "codigo"; size: 24; color: Theme.primary }
                        TextField {
                            id: scanField
                            Layout.fillWidth: true
                            focus: true
                            placeholderText: qsTr("Bipe o código de barras ou digite o nome — Enter adiciona")
                            placeholderTextColor: Theme.textMuted
                            color: Theme.text
                            font.family: Theme.fontBase
                            font.pixelSize: Theme.fontLg
                            background: Item {}
                            onTextChanged: tela.atualizarSugestoes(text)
                            onAccepted: tela.processarEntrada()
                        }
                        Text {
                            id: avisoScan
                            property string _t: ""
                            function mostrar(t) { _t = t; limpaTimer.restart(); }
                            text: _t
                            color: Theme.danger
                            font.pixelSize: Theme.fontSm
                            Timer { id: limpaTimer; interval: 2500; onTriggered: avisoScan._t = "" }
                        }
                    }

                    // Sugestões (busca por nome)
                    Popup {
                        id: sugPopup
                        y: parent.height + 2
                        x: 0
                        width: parent.width
                        padding: 4
                        visible: sugestoes.count > 0 && scanField.activeFocus
                        closePolicy: Popup.NoAutoClose
                        background: Rectangle {
                            radius: Theme.radiusSm
                            color: Theme.surface
                            border.color: Theme.border
                        }
                        contentItem: ListView {
                            implicitHeight: Math.min(contentHeight, 240)
                            clip: true
                            model: sugestoes
                            delegate: ItemDelegate {
                                width: ListView.view.width
                                required property int index
                                required property string nome
                                required property var preco
                                contentItem: RowLayout {
                                    Text { text: nome; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; elide: Text.ElideRight }
                                    Text { text: App.formatarDinheiro(preco); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                                }
                                onClicked: {
                                    tela.adicionar(sugestoes.get(index));
                                    scanField.clear();
                                    sugestoes.clear();
                                    scanField.forceActiveFocus();
                                }
                            }
                        }
                    }
                }

                // Aviso de estoque insuficiente (não bloqueia a venda)
                Rectangle {
                    Layout.fillWidth: true
                    visible: tela.avisoEstoque.length > 0
                    radius: Theme.radiusSm
                    color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.15)
                    border.color: Theme.warning
                    implicitHeight: avisoEstoqueTxt.implicitHeight + 2 * Theme.spacingSm
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingSm
                        Text { text: "⚠"; color: Theme.warning; font.pixelSize: Theme.fontMd }
                        Text {
                            id: avisoEstoqueTxt
                            Layout.fillWidth: true
                            text: tela.avisoEstoque
                            color: Theme.warning
                            font.pixelSize: Theme.fontSm
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // Carrinho
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
                                Text { text: qsTr("Produto"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                Text { text: qsTr("Qtd"); Layout.preferredWidth: 110; horizontalAlignment: Text.AlignHCenter; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                Text { text: qsTr("Preço"); visible: tela.mostrarPrecoUnit; Layout.preferredWidth: 90; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                Text { text: qsTr("Subtotal"); visible: tela.mostrarSubtotal; Layout.preferredWidth: 96; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                                Item { Layout.preferredWidth: 28 }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                        ListView {
                            id: cartView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onWidthChanged: tela.larguraCarrinho = width
                            clip: true
                            model: cart
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                id: linha
                                required property int index
                                required property string nome
                                required property int embId
                                required property string embNome
                                required property string insumosLabel
                                required property string embalagensJson
                                required property var preco
                                required property int qtd
                                required property int desconto
                                readonly property var _embs: JSON.parse(linha.embalagensJson && linha.embalagensJson.length ? linha.embalagensJson : "[]")
                                readonly property bool _composto: linha.insumosLabel.length > 0
                                width: ListView.view.width
                                height: 58
                                color: "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingMd
                                    anchors.rightMargin: Theme.spacingMd
                                    spacing: Theme.spacingSm

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: linha.nome; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                        // Composto: mostra os insumos escolhidos.
                                        Text {
                                            visible: linha._composto
                                            text: linha.insumosLabel
                                            Layout.fillWidth: true; elide: Text.ElideRight
                                            color: Theme.textMuted; font.pixelSize: Theme.fontXs
                                        }
                                        // Simples com várias embalagens: seletor unidade/caixa/fardo.
                                        AppComboBox {
                                            visible: !linha._composto && linha._embs.length > 1
                                            Layout.preferredWidth: 160
                                            model: linha._embs
                                            textRole: "nome"
                                            valueRole: "id"
                                            Component.onCompleted: currentIndex = indexOfValue(linha.embId)
                                            onActivated: {
                                                var e = linha._embs[currentIndex];
                                                if (e) {
                                                    cart.setProperty(linha.index, "embId", e.id);
                                                    cart.setProperty(linha.index, "embNome", e.nome);
                                                    cart.setProperty(linha.index, "fator", e.fator);
                                                    cart.setProperty(linha.index, "preco", e.preco);
                                                    tela.recomputar();
                                                }
                                            }
                                        }
                                        // Simples com uma embalagem: só o nome.
                                        Text {
                                            visible: !linha._composto && linha._embs.length <= 1
                                            text: linha.embNome
                                            Layout.fillWidth: true; elide: Text.ElideRight
                                            color: Theme.textMuted; font.pixelSize: Theme.fontXs
                                        }
                                    }
                                    AppSpinBox {
                                        Layout.preferredWidth: 110
                                        from: 1; to: 100000
                                        value: linha.qtd
                                        // Permite digitar a quantidade (ex.: 24) além dos botões − / +.
                                        onValueModified: { cart.setProperty(linha.index, "qtd", value); tela.recomputar(); }
                                    }
                                    Text { visible: tela.mostrarPrecoUnit; Layout.preferredWidth: 90; text: App.formatarDinheiro(linha.preco); horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                                    Text { visible: tela.mostrarSubtotal; Layout.preferredWidth: 96; text: App.formatarDinheiro(linha.qtd * linha.preco - linha.desconto); horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                    AppButton { kind: "ghost"; text: "✕"; implicitWidth: 28; onClicked: { cart.remove(linha.index); tela.recomputar(); } }
                                }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: cart.count === 0
                                text: qsTr("Bipe um produto para começar a venda.")
                                color: Theme.textMuted
                            }
                        }
                    }
                }
            }

            // -------------------- Direita: total + pagamento --------------------
            Rectangle {
                Layout.preferredWidth: 380
                Layout.minimumWidth: 300
                Layout.fillHeight: true
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingSm

                    // Cliente
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("Cliente:"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                        Text { text: tela.clienteNome; Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        AppButton { kind: "ghost"; text: qsTr("Trocar (F8)"); onClicked: clienteDialog.abrir() }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.bottomMargin: Theme.spacingXs }

                    // Total
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: qsTr("TOTAL"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; font.letterSpacing: 1 }
                        Text {
                            text: App.formatarDinheiro(tela.totalVenda)
                            color: Theme.primary
                            font.family: Theme.fontBase
                            font.pixelSize: 34
                            font.weight: Font.Bold
                        }
                    }

                    // Desconto — some inteiro para quem não pode dar desconto,
                    // em vez de aparecer e recusar só na hora de finalizar.
                    RowLayout {
                        Layout.fillWidth: true
                        visible: tela.podeDarDesconto
                        Text { text: qsTr("Desconto (F4)"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                        AppTextField {
                            id: descontoField
                            Layout.preferredWidth: 120
                            placeholderText: qsTr("0,00")
                            horizontalAlignment: Text.AlignRight
                            onTextChanged: {
                                var v = App.parseDinheiro(text);
                                tela.descontoGeral = (text.trim().length === 0 || v < 0) ? 0 : v;
                                tela.recomputar();
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: Theme.spacingXs; Layout.bottomMargin: Theme.spacingXs }

                    // Pagamento
                    Text { text: qsTr("Pagamento"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm
                        AppTextField {
                            id: valorField
                            Layout.fillWidth: true
                            placeholderText: qsTr("valor (vazio = falta)")
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: Theme.spacingSm
                        columnSpacing: Theme.spacingSm
                        AppButton { kind: "default"; text: qsTr("Dinheiro"); Layout.fillWidth: true; onClicked: tela.addPagamento("dinheiro") }
                        AppButton { kind: "default"; text: qsTr("Pix"); Layout.fillWidth: true; onClicked: tela.addPagamento("pix") }
                        AppButton { kind: "default"; text: qsTr("Débito"); Layout.fillWidth: true; onClicked: tela.addPagamento("debito") }
                        AppButton { kind: "default"; text: qsTr("Crédito"); Layout.fillWidth: true; onClicked: tela.addPagamento("credito") }
                        AppButton {
                            kind: "default"
                            text: qsTr("Fiado")
                            Layout.fillWidth: true
                            enabled: tela.clienteId > 0
                            onClicked: tela.addPagamento("fiado")
                        }
                    }

                    // Pagamentos lançados
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(contentHeight, 120)
                        clip: true
                        model: pagsModel
                        delegate: RowLayout {
                            width: ListView.view.width
                            required property int index
                            required property string forma
                            required property var valor
                            Text { text: forma.charAt(0).toUpperCase() + forma.slice(1); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd }
                            Text { text: App.formatarDinheiro(valor); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                            AppButton { kind: "ghost"; text: "✕"; implicitWidth: 26; onClicked: { pagsModel.remove(index); tela.recomputar(); } }
                        }
                    }

                    // Falta / troco
                    RowLayout {
                        Layout.fillWidth: true
                        visible: tela.faltaVenda > 0
                        Text { text: qsTr("Falta"); Layout.fillWidth: true; color: Theme.danger; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                        Text { text: App.formatarDinheiro(tela.faltaVenda); color: Theme.danger; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: tela.trocoVenda > 0
                        Text { text: qsTr("Troco"); Layout.fillWidth: true; color: Theme.success; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                        Text { text: App.formatarDinheiro(tela.trocoVenda); color: Theme.success; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                    }

                    Item { Layout.fillHeight: true }

                    AppButton {
                        kind: "accent"
                        text: qsTr("Finalizar venda (F12)")
                        Layout.fillWidth: true
                        enabled: cart.count > 0 && tela.pagoVenda >= tela.totalVenda && tela.totalVenda > 0
                        onClicked: tela.finalizar()
                    }
                    AppButton {
                        kind: "ghost"
                        text: qsTr("Cancelar venda (Esc)")
                        Layout.fillWidth: true
                        onClicked: tela.limparVenda()
                    }
                }
            }
        }

        // Rodapé de atalhos
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 34
            color: Theme.surface
            Rectangle { width: parent.width; height: 1; color: Theme.border }
            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLg
                spacing: Theme.spacingLg
                Text { text: qsTr("F2 Buscar"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                Text { visible: tela.podeDarDesconto; text: qsTr("F4 Desconto"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                Text { text: qsTr("F12 Finalizar"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                Text { text: qsTr("Esc Cancelar"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            }
        }
    }

    // Sucesso
    AppDialog {
        id: sucessoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 440
        padding: Theme.spacingLg
        property int troco: 0
        title: qsTr("Venda concluída")
        standardButtons: Dialog.Ok
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Venda registrada com sucesso!")
                color: Theme.success
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: sucessoDialog.troco > 0
                      ? qsTr("Troco a devolver: ") + App.formatarDinheiro(sucessoDialog.troco)
                      : qsTr("Pagamento exato, sem troco.")
                color: sucessoDialog.troco > 0 ? Theme.primary : Theme.text
                font.pixelSize: Theme.fontXxl
                font.weight: Font.Bold
            }
        }
        onClosed: scanField.forceActiveFocus()
    }

    // Selecionar cliente (para fiado / vínculo da venda)
    AppDialog {
        id: clienteDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 440
        padding: Theme.spacingLg
        function abrir() { App.recarregarClientes(""); buscaCli.text = ""; open(); }
        title: qsTr("Cliente da venda")
        contentItem: ColumnLayout {
            spacing: Theme.spacingSm
            AppTextField {
                id: buscaCli
                Layout.fillWidth: true
                placeholderText: qsTr("Buscar por nome, telefone ou CPF…")
                onTextChanged: App.recarregarClientes(text)
            }
            AppButton {
                kind: "default"
                text: qsTr("Consumidor final (sem cliente)")
                Layout.fillWidth: true
                onClicked: {
                    tela.clienteId = 0;
                    tela.clienteNome = qsTr("Consumidor final");
                    clienteDialog.close();
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                radius: Theme.radiusSm
                color: Theme.surfaceAlt
                border.color: Theme.border
                clip: true
                ListView {
                    anchors.fill: parent
                    clip: true
                    model: App.clientes
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        required property int idCliente
                        required property string nome
                        required property var saldo
                        required property var limite
                        contentItem: RowLayout {
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: nome; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text {
                                    text: qsTr("Deve ") + App.formatarDinheiro(saldo) + qsTr(" · limite ") + App.formatarDinheiro(limite)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontXs
                                }
                            }
                        }
                        onClicked: {
                            tela.clienteId = idCliente;
                            tela.clienteNome = nome;
                            clienteDialog.close();
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: parent.count === 0
                        text: qsTr("Nenhum cliente. Cadastre em Clientes.")
                        color: Theme.textMuted
                    }
                }
            }
            AppButton { kind: "default"; text: qsTr("Fechar"); onClicked: clienteDialog.close() }
        }
    }

    // Montar produto composto (quando a categoria tem mais de um produto)
    AppDialog {
        id: compostoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 540
        padding: Theme.spacingLg
        property var linhas: []
        property var item: null
        property var selecoes: []
        function abrir(it, ls) {
            // Cópia dos campos (it pode ser uma linha de ListModel que será
            // invalidada por sugestoes.clear() antes de confirmarmos).
            item = { produtoId: it.produtoId, nome: it.nome, embalagemId: it.embalagemId,
                     embalagemNome: it.embalagemNome, fator: it.fator, preco: it.preco };
            linhas = ls;
            var s = [];
            for (var i = 0; i < ls.length; i++)
                s.push(ls[i].produtos.length > 0 ? ls[i].produtos[0].id : 0);
            selecoes = s;
            precoComposto.text = App.formatarValor(it.preco || 0);
            erroComposto.text = "";
            open();
        }
        readonly property bool tudoEscolhido: {
            if (linhas.length === 0) return false;
            for (var i = 0; i < linhas.length; i++) {
                if (linhas[i].produtos.length === 0) return false;
                if (!selecoes[i] || selecoes[i] <= 0) return false;
            }
            return true;
        }
        title: qsTr("Montar ") + (item ? item.nome : "")
        contentItem: ScrollView {
            id: rolComposto
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            width: rolComposto.availableWidth
            spacing: Theme.spacingMd
            Text {
                text: qsTr("Monte o pedido escolhendo o produto de cada categoria.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            // Um card por categoria da receita.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                Repeater {
                    model: compostoDialog.linhas
                    delegate: Rectangle {
                        id: compDlgRow
                        required property int index
                        required property var modelData
                        readonly property bool semProduto: compDlgRow.modelData.produtos.length === 0
                        Layout.fillWidth: true
                        implicitHeight: 64
                        radius: Theme.radiusSm
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: compDlgRow.semProduto ? Theme.danger : Theme.border

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spacingMd
                            anchors.rightMargin: Theme.spacingMd
                            spacing: Theme.spacingMd

                            // Badge com a quantidade da receita.
                            Rectangle {
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 46
                                radius: 12
                                color: compDlgRow.semProduto
                                       ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.15)
                                       : Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.15)
                                Column {
                                    anchors.centerIn: parent
                                    spacing: -1
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: compDlgRow.modelData.quantidade
                                        color: compDlgRow.semProduto ? Theme.danger : Theme.primary
                                        font.family: Theme.fontBase
                                        font.pixelSize: Theme.fontMd
                                        font.weight: Font.Bold
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: compDlgRow.modelData.unidade
                                        color: compDlgRow.semProduto ? Theme.danger : Theme.primary
                                        font.pixelSize: 9
                                        font.capitalization: Font.AllUppercase
                                        font.letterSpacing: 0.4
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: compDlgRow.modelData.categoriaNome
                                    color: Theme.text
                                    font.pixelSize: Theme.fontMd
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                AppComboBox {
                                    visible: !compDlgRow.semProduto
                                    Layout.fillWidth: true
                                    model: compDlgRow.modelData.produtos
                                    textRole: "nome"
                                    valueRole: "id"
                                    Component.onCompleted: currentIndex = 0
                                    onActivated: {
                                        var s = compostoDialog.selecoes.slice();
                                        s[compDlgRow.index] = currentValue;
                                        compostoDialog.selecoes = s;
                                    }
                                }
                                Text {
                                    visible: compDlgRow.semProduto
                                    Layout.fillWidth: true
                                    text: qsTr("Nenhum produto cadastrado nesta categoria")
                                    color: Theme.danger
                                    font.pixelSize: Theme.fontSm
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            // Preço em destaque.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: precoRow.implicitHeight + 2 * Theme.spacingMd
                radius: Theme.radiusSm
                color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.10)
                border.width: 1
                border.color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.35)
                RowLayout {
                    id: precoRow
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: Theme.spacingMd
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text { text: qsTr("Preço do copão"); color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                        Text { text: qsTr("Valor cobrado nesta venda"); color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                    }
                    AppTextField {
                        id: precoComposto
                        Layout.preferredWidth: 150
                        horizontalAlignment: Text.AlignRight
                        placeholderText: "0,00"
                    }
                }
            }

            Label {
                id: erroComposto
                Layout.fillWidth: true
                visible: text.length > 0
                color: Theme.danger
                font.pixelSize: Theme.fontSm
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Adicionar ao carrinho")
                    onClicked: {
                        erroComposto.text = "";
                        var escolhas = [];
                        for (var i = 0; i < compostoDialog.linhas.length; i++) {
                            var linha = compostoDialog.linhas[i];
                            if (linha.produtos.length === 0) {
                                erroComposto.text = qsTr("Cadastre um produto na categoria: ") + linha.categoriaNome;
                                return;
                            }
                            var id = compostoDialog.selecoes[i];
                            if (!id || id <= 0)
                                id = linha.produtos[0].id;   // usa o 1º (mostrado por padrão)
                            var nome = "";
                            for (var k = 0; k < linha.produtos.length; k++)
                                if (linha.produtos[k].id === id) nome = linha.produtos[k].nome;
                            escolhas.push({ produtoId: id, nome: nome, quantidade: linha.quantidade });
                        }
                        var preco = App.parseDinheiro(precoComposto.text);
                        if (preco < 0) preco = compostoDialog.item.preco;
                        tela._adicionarComposto(compostoDialog.item, escolhas, preco);
                        compostoDialog.close();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: compostoDialog.close() }
                Item { Layout.fillWidth: true }
            }
            }
        }
    }

    // Sangria / Suprimento
    AppDialog {
        id: movCaixaDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 400
        padding: Theme.spacingLg
        property string tipo: "sangria"
        function abrir(t) { tipo = t; movValor.text = ""; movMotivo.text = ""; movErro.text = ""; open(); }
        title: tipo === "sangria" ? qsTr("Sangria (retirada de dinheiro)")
                                  : qsTr("Suprimento (reforço de dinheiro)")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            FormField {
                label: qsTr("Valor")
                Layout.fillWidth: true
                AppTextField { id: movValor; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" }
            }
            FormField {
                label: qsTr("Motivo")
                Layout.fillWidth: true
                AppTextField { id: movMotivo; width: parent.width; placeholderText: qsTr("ex.: pagamento fornecedor, troco") }
            }
            Label { id: movErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Confirmar")
                    onClicked: {
                        var ok = movCaixaDialog.tipo === "sangria"
                                 ? App.registrarSangria(movValor.text, movMotivo.text)
                                 : App.registrarSuprimento(movValor.text, movMotivo.text);
                        if (ok) movCaixaDialog.close();
                        else movErro.text = App.ultimoErro();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: movCaixaDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Fechamento de caixa
    AppDialog {
        id: fecharDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        property var resumo: ({})
        property int diferenca: 0
        property bool contadoValido: false
        function abrir() {
            resumo = App.caixaResumo();
            contadoField.text = "";
            diferenca = 0;
            contadoValido = false;
            fecharErro.text = "";
            open();
        }
        function recalcular() {
            var v = App.parseDinheiro(contadoField.text);
            contadoValido = contadoField.text.trim().length > 0 && v >= 0;
            diferenca = (v < 0 ? 0 : v) - (resumo.dinheiroEsperado || 0);
        }
        title: qsTr("Fechar caixa")
        contentItem: ScrollView {
            id: rolFechar
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
            width: rolFechar.availableWidth
            spacing: Theme.spacingXs

            // --- Bloco informativo: o que foi vendido no turno ---
            KV { k: qsTr("Vendas do turno (%1)").arg(fecharDialog.resumo.numVendas || 0); v: App.formatarDinheiro(fecharDialog.resumo.totalVendas || 0); cor: Theme.text }
            KV { k: qsTr("  Dinheiro"); v: App.formatarDinheiro(fecharDialog.resumo.vendasDinheiro || 0) }
            KV { k: qsTr("  Pix"); v: App.formatarDinheiro(fecharDialog.resumo.vendasPix || 0) }
            KV { k: qsTr("  Débito"); v: App.formatarDinheiro(fecharDialog.resumo.vendasDebito || 0) }
            KV { k: qsTr("  Crédito"); v: App.formatarDinheiro(fecharDialog.resumo.vendasCredito || 0) }
            KV { k: qsTr("  Fiado"); v: App.formatarDinheiro(fecharDialog.resumo.vendasFiado || 0) }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 2
                wrapMode: Text.WordWrap
                text: qsTr("Pix, cartão e fiado não entram na gaveta — só o dinheiro é conferido abaixo.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }

            // --- Bloco do dinheiro: é isto que forma o esperado ---
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: 8; Layout.bottomMargin: 6 }
            Text {
                text: qsTr("DINHEIRO NA GAVETA")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                font.letterSpacing: 0.8
                Layout.bottomMargin: 2
            }
            KV { k: qsTr("Abertura (troco inicial)"); v: "+ " + App.formatarDinheiro(fecharDialog.resumo.abertura || 0) }
            KV { k: qsTr("Vendas em dinheiro"); v: "+ " + App.formatarDinheiro(fecharDialog.resumo.vendasDinheiro || 0) }
            KV { k: qsTr("Suprimentos"); v: "+ " + App.formatarDinheiro(fecharDialog.resumo.suprimentos || 0) }
            KV { k: qsTr("Recebimentos de fiado"); v: "+ " + App.formatarDinheiro(fecharDialog.resumo.recebimentos || 0) }
            KV { k: qsTr("Troco devolvido"); v: "− " + App.formatarDinheiro(fecharDialog.resumo.troco || 0) }
            KV { k: qsTr("Sangrias / pagamentos"); v: "− " + App.formatarDinheiro(fecharDialog.resumo.sangrias || 0) }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: 4; Layout.bottomMargin: 4 }
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("Dinheiro esperado na gaveta"); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text { text: App.formatarDinheiro(fecharDialog.resumo.dinheiroEsperado || 0); color: Theme.primary; font.pixelSize: Theme.fontLg; font.weight: Font.Bold }
            }

            FormField {
                label: qsTr("Dinheiro contado na gaveta")
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingSm
                AppTextField {
                    id: contadoField
                    width: parent.width
                    horizontalAlignment: Text.AlignRight
                    placeholderText: "0,00"
                    onTextChanged: fecharDialog.recalcular()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: fecharDialog.contadoValido
                Text { text: qsTr("Diferença"); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text {
                    text: App.formatarDinheiro(fecharDialog.diferenca)
                    color: fecharDialog.diferenca === 0 ? Theme.success
                         : (fecharDialog.diferenca < 0 ? Theme.danger : Theme.warning)
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                }
            }
            Label { id: fecharErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingSm
                AppButton {
                    kind: "accent"
                    text: qsTr("Fechar caixa")
                    enabled: fecharDialog.contadoValido
                    onClicked: {
                        var r = App.fecharCaixa(contadoField.text);
                        if (r.ok) {
                            fecharDialog.close();
                            resultadoDialog.r = r;
                            resultadoDialog.open();
                        } else {
                            fecharErro.text = r.erro;
                        }
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: fecharDialog.close() }
                Item { Layout.fillWidth: true }
            }
            }
        }
    }

    // Resultado do fechamento
    AppDialog {
        id: resultadoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 380
        padding: Theme.spacingLg
        property var r: ({})
        title: qsTr("Caixa fechado")
        standardButtons: Dialog.Ok
        contentItem: ColumnLayout {
            spacing: Theme.spacingXs
            KV { k: qsTr("Esperado"); v: App.formatarDinheiro(resultadoDialog.r.esperado || 0); cor: Theme.text }
            KV { k: qsTr("Contado"); v: App.formatarDinheiro(resultadoDialog.r.informado || 0); cor: Theme.text }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Text { text: qsTr("Diferença"); Layout.fillWidth: true; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text {
                    text: App.formatarDinheiro(resultadoDialog.r.diferenca || 0)
                    color: (resultadoDialog.r.diferenca || 0) === 0 ? Theme.success
                         : ((resultadoDialog.r.diferenca || 0) < 0 ? Theme.danger : Theme.warning)
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
