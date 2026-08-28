import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Tela de Estoque: lista com status + diálogo de Entrada de mercadoria e
// Inventário. Quantidades sempre em unidade base; entrada aceita embalagem.
Rectangle {
    id: tela
    color: Theme.background

    readonly property int colLoc: 150
    readonly property int colQtd: 96
    readonly property int colMin: 84
    readonly property int colCusto: 116
    readonly property int colStatus: 96

    function abrirMov(produtoId) {
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
                text: qsTr("Clique num produto para dar entrada ou inventariar")
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

                        width: ListView.view.width
                        height: 48
                        leftPadding: Theme.spacingMd
                        rightPadding: Theme.spacingMd
                        onClicked: tela.abrirMov(idProduto)

                        contentItem: RowLayout {
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
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
                            Text { Layout.preferredWidth: tela.colMin; text: linha.minimo; horizontalAlignment: Text.AlignRight; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
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
            contagemSpin.value = atual.quantidade !== undefined ? atual.quantidade : 0;
            motivoField.text = "";
            retEmbCombo.currentIndex = 0;
            retQtdSpin.value = 1;
            retMotivoField.text = "";
            tabs.currentIndex = 0;
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

            SegmentedControl {
                id: tabs
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                options: [qsTr("Entrada"), qsTr("Inventário"), qsTr("Retirada")]
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
                    onClicked: {
                        var ok;
                        if (tabs.currentIndex === 0)
                            ok = App.registrarEntrada(movDialog.produtoId, embCombo.currentValue,
                                                      qtdSpin.value, custoField.text, obsField.text);
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
