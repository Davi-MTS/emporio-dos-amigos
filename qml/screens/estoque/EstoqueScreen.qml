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
    // Margem é lucro da loja: vai junto com o resto do financeiro. O
    // funcionário recebe mercadoria e consulta saldo, mas não vê quanto a loja
    // ganha em cada produto. A coluna some inteira (cabeçalho e célula) — um
    // "—" no lugar do número diria que existe algo escondido ali.
    readonly property bool podeVerMargem: _perms.tudo === true || _perms.ve_financeiro === true

    readonly property int colLoc: 150
    readonly property int colQtd: 96
    readonly property int colMin: 108
    readonly property int colCusto: 116
    readonly property int colMargem: 104
    readonly property int colStatus: 96

    // Colunas que somem quando a janela encolhe, na ordem inversa da utilidade
    // — mesmo recurso que a tela de Produtos já usa. Sem isto, na janela
    // restaurada as sete colunas não cabiam e o cabeçalho escrevia "Produto" e
    // "Localização" um por cima do outro.
    //
    // O que NUNCA some: o nome do produto e a quantidade — é para isso que se
    // abre esta tela. Localização é a primeira a sair (na loja está vazia na
    // maioria dos produtos); depois o mínimo, que só serve para explicar o selo.
    readonly property bool mostrarStatus: listaBox.width > 420
    readonly property bool mostrarCusto:  listaBox.width > 560
    readonly property bool mostrarMargem: podeVerMargem && listaBox.width > 780
    readonly property bool mostrarMinimo: listaBox.width > 920
    readonly property bool mostrarLoc:    listaBox.width > 1080

    // Encolheu a ponto de a coluna sumir: o filtro de margem sai junto e é
    // desligado, senão a lista ficaria filtrada por algo invisível.
    onMostrarMargemChanged: if (!mostrarMargem) App.estoque.filtroMargem = ""

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

    // Filtro por faixa de margem. As faixas são do dono (abaixo de 35% baixa,
    // 35 a 45 boa, acima de 45 muito boa) e vivem no model: a mesma regra pinta
    // o número na coluna e filtra a lista.
    readonly property var chavesMargem: ["", "baixa", "boa", "muitoboa"]
    readonly property var nomesMargem: ({ "": qsTr("Todas"), "baixa": qsTr("Baixa"),
                                          "boa": qsTr("Boa"), "muitoboa": qsTr("Muito boa") })
    readonly property var contagemMargem: App.estoque.contagemMargem
    function rotuloMargem(chave) {
        var n = tela.contagemMargem[chave === "" ? "todos" : chave];
        return tela.nomesMargem[chave] + "  " + (n !== undefined ? n : 0);
    }
    // A coluna Margem NÃO usa as cores do selo de Status. O selo já fala em
    // verde/âmbar/vermelho ali do lado, e duas conversas de cor coladas uma na
    // outra faziam ler "34,3% âmbar" junto com "⚠ Baixo âmbar" como se fossem a
    // mesma coisa — e não são: uma é lucro, a outra é quantidade em estoque.
    // A faixa aparece escrita embaixo do número, em tom apagado.
    function corDaMargem(decimos) {
        if (decimos === undefined)
            return Theme.textMuted;
        // Prejuízo é a única exceção: o número já vem com sinal de menos, e
        // vermelho aqui não compete com selo nenhum (nenhum produto em
        // prejuízo fica verde ao lado).
        return decimos < 0 ? Theme.danger : Theme.text;
    }
    function nomeDaFaixa(decimos) {
        if (decimos === undefined)
            return "";
        if (decimos < 0)
            return qsTr("prejuízo");
        if (decimos < 350)
            return qsTr("baixa");
        if (decimos <= 450)
            return qsTr("boa");
        return qsTr("muito boa");
    }

    // Os filtros vivem no model, que é um só para o app: sair da tela com
    // "Zerados" ou "Baixa" ligado faria a próxima visita abrir escondendo
    // produtos, sem nada na tela dizendo por quê.
    Component.onDestruction: {
        App.estoque.filtroStatus = "";
        App.estoque.filtroMargem = "";
    }

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
            // Os dois filtros andam JUNTOS: num Row só, eles passam para a
            // linha de baixo de uma vez e sempre alinhados um com o outro. Soltos
            // no Flow, um ficava em cima e outro embaixo, com larguras
            // diferentes — parecia desalinho, não organização.
            Row {
                id: grupoFiltros
                spacing: Theme.spacingSm
                // Cada um pede a largura natural das suas legendas e cede o que
                // for preciso para os dois caberem na barra.
                readonly property real _cabe: (barraEstoque.width - spacing) / 2
                // Urgência primeiro: o que zerou, depois o que está acabando.
                SegmentedControl {
                    id: filtroStatus
                    objectName: "filtroStatusEstoque"
                    width: Math.min(implicitWidth, tela.podeVerMargem
                                                   ? grupoFiltros._cabe : barraEstoque.width)
                    height: 40
                    options: tela.chavesFiltro.map(function (c) { return tela.rotuloFiltro(c); })
                    currentIndex: Math.max(0, tela.chavesFiltro.indexOf(App.estoque.filtroStatus))
                    onCurrentIndexChanged: App.estoque.filtroStatus = tela.chavesFiltro[currentIndex]
                }
                // Margem: some junto com a coluna para quem não vê o financeiro.
                SegmentedControl {
                    id: filtroMargem
                    objectName: "filtroMargemEstoque"
                    // Anda com a COLUNA: filtrar por uma faixa que a tela não
                    // está mostrando deixaria a lista encurtada sem nada
                    // explicando. Por isso some junto — e o filtro é desligado.
                    visible: tela.mostrarMargem
                    width: visible ? Math.min(implicitWidth, grupoFiltros._cabe) : 0
                    height: 40
                    options: tela.chavesMargem.map(function (c) { return tela.rotuloMargem(c); })
                    currentIndex: Math.max(0, tela.chavesMargem.indexOf(App.estoque.filtroMargem))
                    onCurrentIndexChanged: App.estoque.filtroMargem = tela.chavesMargem[currentIndex]
                }
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
            id: listaBox
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
                        Text { text: qsTr("Produto"); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { text: qsTr("Localização"); visible: tela.mostrarLoc; Layout.preferredWidth: tela.colLoc; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { objectName: "cabQtd"; text: qsTr("Qtd atual"); Layout.preferredWidth: tela.colQtd; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { objectName: "cabMinimo"; text: qsTr("Mínimo"); visible: tela.mostrarMinimo; Layout.preferredWidth: tela.colMin; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        Text { objectName: "cabCusto"; text: qsTr("Custo médio"); visible: tela.mostrarCusto; Layout.preferredWidth: tela.colCusto; elide: Text.ElideRight; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        // A folga à direita é o que separa a margem do selo de
                        // status; sem ela "42,6%" e "OK" ficam colados.
                        Text { objectName: "margemCabecalho"; text: qsTr("Margem"); visible: tela.mostrarMargem; Layout.preferredWidth: tela.colMargem; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                        // Cerca entre os números e a situação do estoque: são
                        // duas leituras diferentes e estavam encostadas.
                        Rectangle { objectName: "divisorStatus"; visible: tela.mostrarStatus; Layout.preferredWidth: 1; Layout.preferredHeight: 20; Layout.leftMargin: Theme.spacingMd; Layout.rightMargin: Theme.spacingMd; color: Theme.border }
                        Text { text: qsTr("Status"); visible: tela.mostrarStatus; Layout.preferredWidth: tela.colStatus; color: Theme.textMuted; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

                ListView {
                    id: lista
                    objectName: "listaEstoque"
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
                        required property var margem
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
                            Text { visible: tela.mostrarLoc; Layout.preferredWidth: tela.colLoc; text: linha.localizacao; elide: Text.ElideRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                            Text { objectName: "qtdLinha"; Layout.preferredWidth: tela.colQtd; text: linha.quantidade + " " + linha.unidadeBase; horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                            // Com a unidade junto, igual à "Qtd atual". Um "5" solo ao lado de
                            // "600 ml" se lê como 5 garrafas — e são 5 ml.
                            Text {
                                objectName: "minimoLinha"
                                visible: tela.mostrarMinimo
                                Layout.preferredWidth: tela.colMin
                                text: linha.minimo > 0 ? (linha.minimo + " " + linha.unidadeBase) : "—"
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMd
                            }
                            Text { objectName: "custoLinha"; visible: tela.mostrarCusto; Layout.preferredWidth: tela.colCusto; text: App.formatarDinheiro(linha.custoMedio); horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: Theme.fontMd }
                            // Margem sobre o PREÇO DE VENDA, não markup: custo R$ 10,00 e
                            // venda R$ 15,00 => 33,3%. Sem preço de venda ou com custo
                            // desconhecido não há o que calcular, e "—" diz isso sem
                            // inventar 0% ou 100%. Prejuízo (custo acima do preço) sai em
                            // vermelho: é a linha que precisa ser olhada hoje.
                            // A faixa vem ESCRITA embaixo do número, apagada, em
                            // vez de pintada nele: assim a coluna não disputa
                            // leitura com o selo de Status, que é colorido.
                            ColumnLayout {
                                visible: tela.mostrarMargem
                                Layout.preferredWidth: tela.colMargem
                                // OBRIGATÓRIO: uma Layout dentro de outra tem
                                // fillWidth TRUE por padrão (item comum tem
                                // false). Sem isto ela come a folga da linha,
                                // fica com o triplo da largura e empurra Qtd,
                                // Mínimo e Custo para longe do cabeçalho.
                                Layout.fillWidth: false
                                spacing: 0
                                Text {
                                    objectName: "margemLinha"
                                    Layout.fillWidth: true
                                    text: linha.margem === undefined ? "—" : App.formatarPercentual(linha.margem)
                                    horizontalAlignment: Text.AlignRight
                                    color: tela.corDaMargem(linha.margem)
                                    font.pixelSize: Theme.fontMd
                                    font.weight: (linha.margem !== undefined && linha.margem < 0) ? Font.DemiBold : Font.Normal
                                }
                                Text {
                                    objectName: "faixaLinha"
                                    Layout.fillWidth: true
                                    visible: text.length > 0
                                    text: tela.nomeDaFaixa(linha.margem)
                                    horizontalAlignment: Text.AlignRight
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontXs
                                }
                            }
                            Rectangle { visible: tela.mostrarStatus; Layout.preferredWidth: 1; Layout.preferredHeight: 28; Layout.leftMargin: Theme.spacingMd; Layout.rightMargin: Theme.spacingMd; color: Theme.border }
                            Item {
                                visible: tela.mostrarStatus
                                Layout.preferredWidth: tela.colStatus
                                implicitHeight: 22
                                StatusBadge { status: linha.status; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }

                        // Fio entre as linhas, igual ao das Compras: só para o
                        // olho não escorregar de uma linha para a outra numa
                        // lista de 280 produtos.
                        Rectangle {
                            objectName: "separadorLinha"
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
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

        // Aba Custo: o que muda ANTES de gravar (custo e margem antes -> depois,
        // quantas vendas seriam corrigidas). Recalcula a cada tecla.
        readonly property var previaCusto: App.previaAjusteCusto(
            produtoId, custoEmbCombo.currentValue !== undefined ? custoEmbCombo.currentValue : 0,
            custoNovoField.text)
        // Mesma régua do aviso da Entrada. Aqui ela é a que separa custo errado
        // de FATOR errado: se o custo "certo" ainda der uma margem absurda, o
        // problema está no cadastro da embalagem, e mexer no custo pioraria.
        readonly property var avisoCustoAjuste: App.avaliarCusto(
            produtoId, custoEmbCombo.currentValue !== undefined ? custoEmbCombo.currentValue : 0,
            custoNovoField.text)
        readonly property bool temAvisoCustoAjuste: avisoCustoAjuste.nivel !== undefined
                                                    && avisoCustoAjuste.nivel.length > 0
        property bool custoAjusteConferido: false
        onAvisoCustoAjusteChanged: custoAjusteConferido = false

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
            custoEmbCombo.currentIndex = 0;
            custoNovoField.text = "";
            custoMotivoField.text = "";
            corrigirVendasCusto.checked = true;
            custoAjusteConferido = false;
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
                // Flow, e não RowLayout: com preço e margem são quatro blocos, e
                // na janela restaurada eles não cabem numa linha só — o Flow
                // quebra em vez de empurrar o último para fora do diálogo.
                Flow {
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
                    // O preço de referência fica AO LADO da margem de propósito:
                    // é o preço da menor embalagem com preço (a unidade), e sem
                    // ele escrito a margem vira um número sem origem.
                    Column {
                        visible: movDialog.atual.precoBase !== undefined && movDialog.atual.precoBase > 0
                        Text { text: qsTr("Preço de venda"); color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
                        Text {
                            text: App.formatarDinheiro(movDialog.atual.precoBase !== undefined ? movDialog.atual.precoBase : 0)
                                  + " / " + (movDialog.atual.unidadeBase !== undefined ? movDialog.atual.unidadeBase : "")
                            color: Theme.text; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold
                        }
                    }
                    Column {
                        objectName: "margemDialogo"
                        visible: tela.podeVerMargem && movDialog.atual.margem !== undefined
                        Text { text: qsTr("Margem"); color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
                        Text {
                            objectName: "margemValor"
                            text: movDialog.atual.margem !== undefined
                                  ? App.formatarPercentual(movDialog.atual.margem)
                                    + " · " + tela.nomeDaFaixa(movDialog.atual.margem)
                                  : ""
                            color: tela.corDaMargem(movDialog.atual.margem)
                            font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold
                        }
                    }
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
                         ? [qsTr("Entrada"), qsTr("Inventário"), qsTr("Retirada"), qsTr("Custo")]
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

                // --- Custo: corrige custo lançado errado, sem mexer na quantidade ---
                ColumnLayout {
                    objectName: "abaCusto"
                    spacing: Theme.spacingSm
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd
                        FormField {
                            label: qsTr("Embalagem")
                            Layout.fillWidth: true
                            AppComboBox {
                                id: custoEmbCombo
                                objectName: "custoEmbCombo"
                                width: parent.width
                                model: movDialog.embalagens
                                textRole: "nome"
                                valueRole: "id"
                            }
                        }
                        FormField {
                            objectName: "rotuloCustoNovo"
                            // Sem artigo de propósito: "do Caixinha", "da FARDO"...
                            // o nome da embalagem é livre e o gênero erraria.
                            label: qsTr("Novo custo (%1)").arg(custoEmbCombo.currentText)
                            Layout.preferredWidth: 170
                            AppTextField {
                                id: custoNovoField
                                objectName: "custoNovoField"
                                width: parent.width
                                horizontalAlignment: Text.AlignRight
                                // Vazio, o campo mostra o custo ATUAL da embalagem
                                // escolhida: trocar para o Fardo mostra o do fardo.
                                placeholderText: movDialog.previaCusto.custoAtualEmbalagem !== undefined
                                                 ? App.formatarValor(movDialog.previaCusto.custoAtualEmbalagem)
                                                 : "0,00"
                            }
                        }
                    }

                    // O custo de CADA embalagem, antes e depois. Todas saem do
                    // mesmo custo por unidade (é o mesmo estoque), então corrigir
                    // o fardo corrige a unidade junto — a tabela mostra isso em
                    // vez de deixar para adivinhar.
                    ColumnLayout {
                        id: tabelaCusto
                        objectName: "tabelaCustoEmbalagens"
                        Layout.fillWidth: true
                        spacing: 2
                        readonly property string abrev: movDialog.previaCusto.unidadeBase === "unidade"
                                                        ? qsTr("un.") : (movDialog.previaCusto.unidadeBase || "")
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            Text { text: qsTr("Embalagem"); Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                            Text { text: qsTr("Atual"); Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                            Text { text: qsTr("Novo"); Layout.preferredWidth: 100; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                        }
                        Repeater {
                            model: movDialog.previaCusto.embalagens || []
                            delegate: RowLayout {
                                id: linhaCusto
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                // A embalagem escolhida em destaque: é a do campo.
                                readonly property bool destaque: modelData.escolhida === true
                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    elide: Text.ElideRight
                                    text: linhaCusto.modelData.nome
                                          + (linhaCusto.modelData.fator > 1
                                             ? " (" + linhaCusto.modelData.fator + " " + tabelaCusto.abrev + ")" : "")
                                    color: linhaCusto.destaque ? Theme.text : Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                    font.weight: linhaCusto.destaque ? Font.DemiBold : Font.Normal
                                }
                                Text {
                                    Layout.preferredWidth: 100
                                    horizontalAlignment: Text.AlignRight
                                    text: App.formatarDinheiro(linhaCusto.modelData.atual)
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                }
                                Text {
                                    Layout.preferredWidth: 100
                                    horizontalAlignment: Text.AlignRight
                                    text: linhaCusto.modelData.novo !== undefined
                                          ? App.formatarDinheiro(linhaCusto.modelData.novo) : "—"
                                    color: linhaCusto.modelData.novo !== undefined ? Theme.text : Theme.textMuted
                                    font.pixelSize: Theme.fontSm
                                    font.weight: linhaCusto.destaque ? Font.DemiBold : Font.Normal
                                }
                            }
                        }
                    }
                    Text {
                        objectName: "previaMargemTexto"
                        Layout.fillWidth: true
                        visible: movDialog.previaCusto.valido === true
                                 && movDialog.previaCusto.margemNova !== undefined
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                        readonly property var pv: movDialog.previaCusto
                        text: {
                            var antes = pv.margemAtual !== undefined ? App.formatarPercentual(pv.margemAtual) : "—";
                            var depois = pv.margemNova !== undefined ? App.formatarPercentual(pv.margemNova) : "—";
                            return qsTr("Margem: %1 → %2").arg(antes).arg(depois);
                        }
                    }
                    Label {
                        objectName: "avisoCustoAjuste"
                        Layout.fillWidth: true
                        visible: movDialog.temAvisoCustoAjuste
                        text: movDialog.temAvisoCustoAjuste ? "⚠ " + movDialog.avisoCustoAjuste.mensagem : ""
                        color: Theme.warning
                        font.pixelSize: Theme.fontSm
                        wrapMode: Text.WordWrap
                    }

                    // As vendas que já saíram com o custo errado. Ligado por
                    // padrão: é para isso que se corrige — senão o lucro desses
                    // dias continua mostrando o prejuízo que nunca existiu.
                    ToggleButton {
                        id: corrigirVendasCusto
                        objectName: "corrigirVendasCusto"
                        visible: (movDialog.previaCusto.vendas || 0) > 0
                        checked: true
                        text: {
                            var n = movDialog.previaCusto.vendas || 0;
                            var desde = movDialog.previaCusto.desde || "";
                            return (n === 1 ? qsTr("Corrigir também a venda feita")
                                            : qsTr("Corrigir também as %1 vendas feitas").arg(n))
                                   + (desde.length > 0 ? qsTr(" desde %1").arg(desde) : "");
                        }
                    }

                    FormField {
                        label: qsTr("Motivo (opcional)")
                        Layout.fillWidth: true
                        AppTextField {
                            id: custoMotivoField
                            width: parent.width
                            placeholderText: qsTr("ex.: custo da caixa lançado na unidade")
                        }
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
                    text: tabs.currentIndex === 3
                          ? (movDialog.custoAjusteConferido ? qsTr("Ajustar mesmo assim") : qsTr("Ajustar custo"))
                          : (tabs.currentIndex === 0 && movDialog.custoConferido
                             ? qsTr("Confirmar mesmo assim") : qsTr("Confirmar"))
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
                        if (tabs.currentIndex === 3 && movDialog.temAvisoCustoAjuste && !movDialog.custoAjusteConferido) {
                            movDialog.custoAjusteConferido = true;
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
                        else if (tabs.currentIndex === 2)
                            ok = App.registrarRetirada(movDialog.produtoId, retEmbCombo.currentValue,
                                                       retQtdSpin.value, retMotivoField.text);
                        else
                            ok = App.ajustarCusto(movDialog.produtoId, custoEmbCombo.currentValue,
                                                  custoNovoField.text,
                                                  corrigirVendasCusto.visible && corrigirVendasCusto.checked,
                                                  custoMotivoField.text);
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
