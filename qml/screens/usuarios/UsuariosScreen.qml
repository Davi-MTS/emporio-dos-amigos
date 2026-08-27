import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Gestão de usuários (somente Administrador). Lista + editor.
Rectangle {
    id: tela
    color: Theme.background

    property var usuarioAtual: null
    property var listaPerfis: App.perfis()

    Component.onCompleted: App.recarregarUsuarios()

    function abrirNovo() { usuarioAtual = App.novoUsuario(); _preencher(); erro.text = ""; }
    function abrirUsuario(id) { usuarioAtual = App.usuario(id); _preencher(); erro.text = ""; }
    function fechar() { usuarioAtual = null; }
    function _preencher() {
        if (!usuarioAtual) return;
        nomeField.text = usuarioAtual.nome || "";
        loginField.text = usuarioAtual.login || "";
        senhaField.text = "";
        perfilCombo.currentIndex = perfilCombo.indexOfValue(usuarioAtual.perfilId || 2);
    }
    function salvar() {
        var dados = { id: usuarioAtual.id || 0, nome: nomeField.text,
                      login: loginField.text, perfilId: perfilCombo.currentValue || 2 };
        if (App.salvarUsuario(dados, senhaField.text))
            fechar();
        else
            erro.text = App.ultimoErro();
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
                Text { text: qsTr("Usuários"); color: Theme.textMuted; font.pixelSize: Theme.fontSm }
                Item { Layout.fillWidth: true }
                AppButton { kind: "accent"; text: qsTr("＋ Novo usuário"); onClicked: tela.abrirNovo() }
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
                    model: App.usuarios
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: linha
                        required property int idUsuario
                        required property string nome
                        required property string login
                        required property string perfil
                        width: ListView.view.width
                        height: 50
                        leftPadding: Theme.spacingMd
                        rightPadding: Theme.spacingMd
                        highlighted: tela.usuarioAtual && tela.usuarioAtual.id === idUsuario
                        onClicked: tela.abrirUsuario(idUsuario)
                        contentItem: RowLayout {
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text { text: linha.nome; color: Theme.text; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }
                                Text { text: "@" + linha.login; color: Theme.textMuted; font.pixelSize: Theme.fontXs }
                            }
                            Text { text: linha.perfil; color: Theme.textMuted; font.pixelSize: Theme.fontMd }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        text: qsTr("Nenhum usuário.")
                        color: Theme.textMuted
                    }
                }
            }
        }

        // Editor
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border

            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingLg
                visible: tela.usuarioAtual === null
                text: qsTr("Selecione um usuário ou clique em “Novo usuário”.")
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Theme.textMuted
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd
                visible: tela.usuarioAtual !== null

                Text {
                    text: (tela.usuarioAtual && tela.usuarioAtual.id > 0) ? qsTr("Editar usuário") : qsTr("Novo usuário")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                FormField {
                    label: qsTr("Nome")
                    Layout.fillWidth: true
                    AppTextField { id: nomeField; width: parent.width }
                }
                FormField {
                    label: qsTr("Login")
                    Layout.fillWidth: true
                    AppTextField { id: loginField; width: parent.width }
                }
                FormField {
                    label: qsTr("Perfil")
                    Layout.fillWidth: true
                    AppComboBox {
                        id: perfilCombo
                        width: parent.width
                        model: tela.listaPerfis
                        textRole: "nome"
                        valueRole: "id"
                    }
                }
                FormField {
                    label: (tela.usuarioAtual && tela.usuarioAtual.id > 0)
                           ? qsTr("Nova senha (vazio = manter)") : qsTr("Senha")
                    Layout.fillWidth: true
                    AppTextField { id: senhaField; width: parent.width; echoMode: TextInput.Password }
                }
                Label {
                    id: erro
                    visible: text.length > 0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    AppButton { kind: "accent"; text: qsTr("Salvar"); onClicked: tela.salvar() }
                    AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: tela.fechar() }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        kind: "default"
                        text: qsTr("Desativar")
                        visible: tela.usuarioAtual && tela.usuarioAtual.id > 0
                        onClicked: { if (App.inativarUsuario(tela.usuarioAtual.id)) tela.fechar(); else erro.text = App.ultimoErro(); }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
