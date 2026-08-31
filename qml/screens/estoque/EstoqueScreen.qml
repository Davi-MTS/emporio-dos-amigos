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

        // Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            AppTextField {
                id: buscaField
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                placeholderText: qsTr("Buscar produto…")
                onTextChanged: App.recarregarEstoque(text)
            }
            Item { Layout.fillWidth: true }
            Label {
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

                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        text: qsTr("Nenhum produto cadastrado.\nCadastre em Produtos para controlar o estoque.")
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
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 480
        modal: true
        padding: Theme.spacingLg

        property int produtoId: 0
        property var embalagens: []
        property var atual: ({})

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
            open();
        }

        // dd/mm/aaaa -> ISO, que é como o banco compara datas.
        function validadeIso() {
            var t = validadeField.text.trim();
            if (t.length === 0)
                return "";
            var m = t.match(/^(\d{1,2})[\/\-.](\d{1,2})[\/\-.](\d{4})$/);
            if (!m)
                return "";
            var d = new Date(parseInt(m[3]), parseInt(m[2]) - 1, parseInt(m[1]));
            if (isNaN(d.getTime()) || d.getMonth() !== parseInt(m[2]) - 1)
                return "";   // 31/02 e afins
            return Qt.formatDate(d, "yyyy-MM-dd");
        }
        function validadeOk() { return validadeIso().length > 0; }

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
                                width: parent.width
                                placeholderText: qsTr("ex.: 62,90 — vazio mantém o custo")
                                horizontalAlignment: Text.AlignRight
                            }
                        }
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
                            AppTextField {
                                id: validadeField
                                width: parent.width
                                placeholderText: qsTr("dd/mm/aaaa")
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
                        color: validadeField.text.trim().length > 0 && !movDialog.validadeOk()
                               ? Theme.danger : Theme.textMuted
                        font.pixelSize: Theme.fontXs
                        text: validadeField.text.trim().length === 0
                              ? qsTr("Informe só para o que vence em prazo curto (doces, salgadinhos). Fica na aba Vencimento.")
                              : (movDialog.validadeOk()
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
                    kind: "accent"
                    text: qsTr("Confirmar")
                    // Data escrita errada viraria entrada sem validade nenhuma,
                    // em silêncio — melhor barrar o botão.
                    enabled: tabs.currentIndex !== 0
                             || validadeField.text.trim().length === 0
                             || movDialog.validadeOk()
                    onClicked: {
                        var ok;
                        if (tabs.currentIndex === 0)
                            ok = tela.podeReceberMercadoria
                                 && App.registrarEntrada(movDialog.produtoId, embCombo.currentValue,
                                                      qtdSpin.value, custoField.text, obsField.text,
                                                      movDialog.validadeIso(), loteField.text);
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
