import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Clientes: cadastro, limite de fiado, saldo devedor e quitação.
Rectangle {
    id: tela
    color: Theme.background

    property var clienteAtual: null

    Component.onCompleted: App.recarregarClientes()

    function abrirNovo() { clienteAtual = App.novoCliente(); _preencher(); erro.text = ""; }
    function abrirCliente(id) { clienteAtual = App.cliente(id); _preencher(); erro.text = ""; }
    function fechar() { clienteAtual = null; }
    function _preencher() {
        if (!clienteAtual) return;
        nomeField.text = clienteAtual.nome || "";
        telField.text = clienteAtual.telefone || "";
        cpfField.text = clienteAtual.cpf || "";
        endField.text = clienteAtual.endereco || "";
        anivField.text = clienteAtual.aniversario || "";
        obsField.text = clienteAtual.observacoes || "";
        limiteField.text = App.formatarValor(clienteAtual.limite || 0);
    }
    function salvar() {
        var lim = App.parseDinheiro(limiteField.text);
        var dados = {
            id: clienteAtual.id || 0, nome: nomeField.text, telefone: telField.text,
            cpf: cpfField.text, endereco: endField.text, aniversario: anivField.text,
            observacoes: obsField.text, limite: lim < 0 ? 0 : lim
        };
        if (App.salvarCliente(dados)) fechar();
        else erro.text = App.ultimoErro();
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Lista
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                AppTextField {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    placeholderText: qsTr("Buscar cliente…")
                    onTextChanged: App.recarregarClientes(text)
                }
                Item { Layout.fillWidth: true }
                AppButton { kind: "accent"; text: qsTr("＋ Novo cliente"); onClicked: tela.abrirNovo() }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                clip: true
                ListView {
                    id: lista
                    anchors.fill: parent
                    clip: true
                    model: App.clientes
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: linha
                        required property int idCliente
                        required property string nome
                        required property string telefone
                        required property var saldo
                        width: ListView.view.width
                        height: 52
                        leftPadding: Theme.spacingMd
                        rightPadding: Theme.spacingMd
                        highlighted: tela.clienteAtual && tela.clienteAtual.id === idCliente
                        onClicked: tela.abrirCliente(idCliente)
                        contentItem: RowLayout {
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: linha.nome; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: linha.telefone; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text {
                                text: linha.saldo > 0 ? qsTr("Deve ") + App.formatarDinheiro(linha.saldo) : qsTr("Em dia")
                                color: linha.saldo > 0 ? Theme.danger : Theme.success
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        text: qsTr("Nenhum cliente cadastrado.")
                        color: Theme.textMuted
                    }
                }
            }
        }

        // Editor
        Rectangle {
            Layout.preferredWidth: 480
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingLg
                visible: tela.clienteAtual === null
                text: qsTr("Selecione um cliente ou clique em “Novo cliente”.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textMuted
            }

            ScrollView {
                id: editorScroll
                anchors.fill: parent
                visible: tela.clienteAtual !== null
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: editorScroll.availableWidth
                    spacing: Theme.spacingMd

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.spacingLg
                        Layout.bottomMargin: 0
                        spacing: 2
                        Text {
                            text: (tela.clienteAtual && tela.clienteAtual.id > 0) ? qsTr("Editar cliente") : qsTr("Novo cliente")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.DemiBold
                        }
                        // Saldo + quitar
                        RowLayout {
                            Layout.fillWidth: true
                            visible: tela.clienteAtual && tela.clienteAtual.id > 0 && (tela.clienteAtual.saldo || 0) > 0
                            Text {
                                text: qsTr("Deve ") + App.formatarDinheiro(tela.clienteAtual ? (tela.clienteAtual.saldo || 0) : 0)
                                color: Theme.danger
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }
                            AppButton {
                                kind: "accent"
                                text: qsTr("Receber pagamento")
                                onClicked: receberDialog.abrir()
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: Theme.spacingLg
                        Layout.rightMargin: Theme.spacingLg
                        spacing: Theme.spacingSm

                        FormField { label: qsTr("Nome *"); Layout.fillWidth: true; AppTextField { id: nomeField; width: parent.width } }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField { label: qsTr("Telefone"); Layout.fillWidth: true; AppTextField { id: telField; width: parent.width } }
                            FormField { label: qsTr("CPF"); Layout.fillWidth: true; AppTextField { id: cpfField; width: parent.width } }
                        }
                        FormField { label: qsTr("Endereço"); Layout.fillWidth: true; AppTextField { id: endField; width: parent.width } }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingMd
                            FormField { label: qsTr("Aniversário"); Layout.fillWidth: true; AppTextField { id: anivField; width: parent.width; placeholderText: qsTr("AAAA-MM-DD") } }
                            FormField { label: qsTr("Limite de fiado"); Layout.fillWidth: true; AppTextField { id: limiteField; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" } }
                        }
                        FormField { label: qsTr("Observações"); Layout.fillWidth: true; AppTextField { id: obsField; width: parent.width } }

                        Label { id: erro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Theme.spacingLg
                        AppButton { kind: "accent"; text: qsTr("Salvar"); onClicked: tela.salvar() }
                        AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: tela.fechar() }
                        Item { Layout.fillWidth: true }
                        AppButton {
                            kind: "default"
                            text: qsTr("Desativar")
                            visible: tela.clienteAtual && tela.clienteAtual.id > 0
                            onClicked: { if (App.inativarCliente(tela.clienteAtual.id)) tela.fechar(); else erro.text = App.ultimoErro(); }
                        }
                    }
                }
            }
        }
    }

    // Receber pagamento de fiado (parcial ou total; dinheiro entra no caixa).
    AppDialog {
        id: receberDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        property int saldo: 0
        function abrir() {
            saldo = tela.clienteAtual ? (tela.clienteAtual.saldo || 0) : 0;
            rcpValor.text = App.formatarValor(saldo);
            formaCombo.currentIndex = 0;
            rcpErro.text = "";
            open();
        }
        title: qsTr("Receber pagamento")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                text: qsTr("Dívida atual: ") + App.formatarDinheiro(receberDialog.saldo)
                color: Theme.textMuted; font.pixelSize: Theme.fontSm
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd
                FormField {
                    label: qsTr("Valor recebido")
                    Layout.fillWidth: true
                    AppTextField { id: rcpValor; width: parent.width; horizontalAlignment: Text.AlignRight; placeholderText: "0,00" }
                }
                FormField {
                    label: qsTr("Forma")
                    Layout.fillWidth: true
                    AppComboBox {
                        id: formaCombo
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
                text: qsTr("Deixe o valor cheio para quitar, ou reduza para pagamento parcial. Só o que for em dinheiro entra na gaveta do caixa.")
                color: Theme.textMuted; font.pixelSize: Theme.fontXs
                Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
            Label { id: rcpErro; visible: text.length > 0; color: Theme.danger; font.pixelSize: Theme.fontSm; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Confirmar recebimento")
                    onClicked: {
                        if (!tela.clienteAtual) return;
                        var r = App.receberDeCliente(tela.clienteAtual.id, rcpValor.text, formaCombo.currentValue);
                        if (r.ok) { tela.clienteAtual = App.cliente(tela.clienteAtual.id); receberDialog.close(); }
                        else rcpErro.text = r.erro;
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: receberDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
