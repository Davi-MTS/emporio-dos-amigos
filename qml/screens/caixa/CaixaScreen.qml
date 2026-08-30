import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Caixa — abertura, movimento do turno, sangria/suprimento e fechamento.
//
// Isto morava dentro do PDV, espremido numa barrinha de 46 px. O caixa é a
// prestação de contas do dia: precisa de espaço para conferir ANTES de fechar,
// não de três botões no canto da tela de venda. O PDV agora só vende.
Rectangle {
    id: tela
    color: Theme.background

    // Pedido de navegação para quem hospeda a tela (Main.qml).
    signal navegar(string rota)

    property var resumo: ({})

    Component.onCompleted: atualizar()
    function atualizar() {
        resumo = App.caixaAberto ? App.caixaResumo() : ({});
    }

    // O resumo tem que acompanhar o que aconteceu no PDV enquanto esta tela
    // esteve fora de vista.
    Connections {
        target: App
        function onCaixaAbertoChanged() { tela.atualizar(); }
    }

    function abrir() {
        if (App.abrirCaixa(aberturaField.text)) {
            abrirErro.text = "";
            atualizar();
        } else {
            abrirErro.text = App.ultimoErro();
        }
    }

    // Linha rótulo/valor do resumo.
    component KV: RowLayout {
        id: kvRoot
        property string k: ""
        property string v: ""
        property color cor: Theme.textMuted
        property bool forte: false
        Layout.fillWidth: true
        Text {
            text: kvRoot.k
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            elide: Text.ElideRight
            color: kvRoot.cor
            font.pixelSize: kvRoot.forte ? Theme.fontMd : Theme.fontSm
            font.weight: kvRoot.forte ? Font.DemiBold : Font.Normal
        }
        Text {
            text: kvRoot.v
            color: kvRoot.cor
            font.pixelSize: kvRoot.forte ? Theme.fontMd : Theme.fontSm
            font.weight: Font.DemiBold
        }
    }

    // ========================== CAIXA FECHADO ==========================
    Item {
        anchors.fill: parent
        visible: !App.caixaAberto

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 2 * Theme.spacingLg)
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
                    text: qsTr("Caixa fechado")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Conte o dinheiro que fica na gaveta para dar troco e informe abaixo. Sem caixa aberto o PDV não vende.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                }
                FormField {
                    label: qsTr("Troco inicial")
                    Layout.fillWidth: true
                    AppTextField {
                        id: aberturaField
                        width: parent.width
                        placeholderText: qsTr("ex.: 100,00")
                        horizontalAlignment: Text.AlignRight
                        onAccepted: tela.abrir()
                    }
                }
                Label {
                    id: abrirErro
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: text.length > 0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSm
                }
                AppButton {
                    kind: "accent"
                    text: qsTr("Abrir caixa")
                    Layout.fillWidth: true
                    onClicked: tela.abrir()
                }
            }
        }
    }

    // ========================== CAIXA ABERTO ==========================
    ScrollView {
        id: rolagem
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        contentWidth: availableWidth
        clip: true
        visible: App.caixaAberto

        ColumnLayout {
            width: rolagem.availableWidth
            spacing: Theme.spacingMd

            // ---- Estado + ações ----
            Rectangle {
                Layout.fillWidth: true
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                implicitHeight: topo.implicitHeight + 2 * Theme.spacingLg

                RowLayout {
                    id: topo
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2
                        RowLayout {
                            spacing: Theme.spacingSm
                            Rectangle { implicitWidth: 10; implicitHeight: 10; radius: 5; color: Theme.success }
                            Text {
                                text: qsTr("Caixa aberto")
                                color: Theme.text
                                font.family: Theme.fontDisplay
                                font.pixelSize: Theme.fontXl
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            elide: Text.ElideRight
                            text: qsTr("Troco inicial ") + App.formatarDinheiro(tela.resumo.abertura || 0)
                                  + "  ·  " + (tela.resumo.numVendas || 0) + qsTr(" vendas no turno")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                    }
                    AppButton { kind: "ghost";  text: "↻"; implicitWidth: 40; onClicked: tela.atualizar() }
                    AppButton { kind: "ghost";  text: qsTr("Sangria");      onClicked: movCaixaDialog.abrir("sangria") }
                    AppButton { kind: "ghost";  text: qsTr("Suprimento");   onClicked: movCaixaDialog.abrir("suprimento") }
                    AppButton { kind: "accent"; text: qsTr("Fechar caixa"); onClicked: fecharDialog.abrir() }
                }
            }

            // ---- O que vendeu × o que tem na gaveta ----
            GridLayout {
                Layout.fillWidth: true
                columns: rolagem.availableWidth > 760 ? 2 : 1
                columnSpacing: Theme.spacingMd
                rowSpacing: Theme.spacingMd

                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.border
                    implicitHeight: colVendas.implicitHeight + 2 * Theme.spacingLg

                    ColumnLayout {
                        id: colVendas
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLg
                        spacing: Theme.spacingXs

                        Text {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            elide: Text.ElideRight
                            text: qsTr("Vendas do turno")
                            color: Theme.text
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.DemiBold
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.bottomMargin: 4 }
                        KV {
                            k: qsTr("Total (%1 vendas)").arg(tela.resumo.numVendas || 0)
                            v: App.formatarDinheiro(tela.resumo.totalVendas || 0)
                            cor: Theme.text
                            forte: true
                        }
                        KV { k: qsTr("Dinheiro"); v: App.formatarDinheiro(tela.resumo.vendasDinheiro || 0) }
                        KV { k: qsTr("Pix");      v: App.formatarDinheiro(tela.resumo.vendasPix || 0) }
                        KV { k: qsTr("Débito");   v: App.formatarDinheiro(tela.resumo.vendasDebito || 0) }
                        KV { k: qsTr("Crédito");  v: App.formatarDinheiro(tela.resumo.vendasCredito || 0) }
                        KV { k: qsTr("Fiado");    v: App.formatarDinheiro(tela.resumo.vendasFiado || 0) }
                        Text {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            wrapMode: Text.WordWrap
                            text: qsTr("Pix, cartão e fiado não entram na gaveta.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.border
                    implicitHeight: colGaveta.implicitHeight + 2 * Theme.spacingLg

                    ColumnLayout {
                        id: colGaveta
                        anchors.fill: parent
                        anchors.margins: Theme.spacingLg
                        spacing: Theme.spacingXs

                        Text {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            elide: Text.ElideRight
                            text: qsTr("Dinheiro na gaveta")
                            color: Theme.text
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.DemiBold
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.bottomMargin: 4 }
                        KV { k: qsTr("Abertura (troco inicial)"); v: "+ " + App.formatarDinheiro(tela.resumo.abertura || 0) }
                        KV { k: qsTr("Vendas em dinheiro");       v: "+ " + App.formatarDinheiro(tela.resumo.vendasDinheiro || 0) }
                        KV { k: qsTr("Suprimentos");              v: "+ " + App.formatarDinheiro(tela.resumo.suprimentos || 0) }
                        KV { k: qsTr("Recebimentos de fiado");    v: "+ " + App.formatarDinheiro(tela.resumo.recebimentos || 0) }
                        KV { k: qsTr("Troco devolvido");          v: "− " + App.formatarDinheiro(tela.resumo.troco || 0) }
                        KV { k: qsTr("Sangrias / pagamentos");    v: "− " + App.formatarDinheiro(tela.resumo.sangrias || 0) }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: 4; Layout.bottomMargin: 4 }
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                elide: Text.ElideRight
                                text: qsTr("Esperado na gaveta")
                                color: Theme.text
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: App.formatarDinheiro(tela.resumo.dinheiroEsperado || 0)
                                color: Theme.primary
                                font.pixelSize: Theme.fontLg
                                font.weight: Font.Bold
                            }
                        }
                    }
                }
            }

            AppButton {
                Layout.fillWidth: true
                kind: "default"
                text: qsTr("Ir para o PDV e vender")
                onClicked: tela.navegar("pdv")
            }
        }
    }

    // ------------------------------------------- Sangria / Suprimento
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
                        if (ok) { movCaixaDialog.close(); tela.atualizar(); }
                        else movErro.text = App.ultimoErro();
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: movCaixaDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // ------------------------------------------------ Fechamento
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

                KV {
                    k: qsTr("Vendas do turno (%1)").arg(fecharDialog.resumo.numVendas || 0)
                    v: App.formatarDinheiro(fecharDialog.resumo.totalVendas || 0)
                    cor: Theme.text
                    forte: true
                }
                KV { k: qsTr("  Dinheiro"); v: App.formatarDinheiro(fecharDialog.resumo.vendasDinheiro || 0) }
                KV { k: qsTr("  Pix");      v: App.formatarDinheiro(fecharDialog.resumo.vendasPix || 0) }
                KV { k: qsTr("  Débito");   v: App.formatarDinheiro(fecharDialog.resumo.vendasDebito || 0) }
                KV { k: qsTr("  Crédito");  v: App.formatarDinheiro(fecharDialog.resumo.vendasCredito || 0) }
                KV { k: qsTr("  Fiado");    v: App.formatarDinheiro(fecharDialog.resumo.vendasFiado || 0) }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    wrapMode: Text.WordWrap
                    text: qsTr("Pix, cartão e fiado não entram na gaveta — só o dinheiro é conferido abaixo.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }

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
                KV { k: qsTr("Vendas em dinheiro");       v: "+ " + App.formatarDinheiro(fecharDialog.resumo.vendasDinheiro || 0) }
                KV { k: qsTr("Suprimentos");              v: "+ " + App.formatarDinheiro(fecharDialog.resumo.suprimentos || 0) }
                KV { k: qsTr("Recebimentos de fiado");    v: "+ " + App.formatarDinheiro(fecharDialog.resumo.recebimentos || 0) }
                KV { k: qsTr("Troco devolvido");          v: "− " + App.formatarDinheiro(fecharDialog.resumo.troco || 0) }
                KV { k: qsTr("Sangrias / pagamentos");    v: "− " + App.formatarDinheiro(fecharDialog.resumo.sangrias || 0) }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: 4; Layout.bottomMargin: 4 }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: qsTr("Dinheiro esperado na gaveta"); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
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
                    Text { text: qsTr("Diferença"); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
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
                                tela.atualizar();
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

    // ------------------------------------- Resultado do fechamento
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
            KV { k: qsTr("Contado");  v: App.formatarDinheiro(resultadoDialog.r.informado || 0); cor: Theme.text }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Text { text: qsTr("Diferença"); Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                Text {
                    text: App.formatarDinheiro(resultadoDialog.r.diferenca || 0)
                    color: (resultadoDialog.r.diferenca || 0) === 0 ? Theme.success
                         : ((resultadoDialog.r.diferenca || 0) < 0 ? Theme.danger : Theme.warning)
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                wrapMode: Text.WordWrap
                text: qsTr("Backup feito e resumo enviado no Telegram, se estiver configurado.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
        }
    }
}
