import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Portão de acesso. No primeiro uso (sem admin com senha), pede a criação do
// administrador; depois, login normal.
Rectangle {
    id: tela
    color: Theme.sidebar

    property bool modoCriar: App.precisaCriarAdmin

    function entrar() {
        erro.text = "";
        if (modoCriar) {
            if (senhaField.text.length < 4) { erro.text = qsTr("A senha deve ter ao menos 4 caracteres."); return; }
            if (senhaField.text !== confirmField.text) { erro.text = qsTr("As senhas não conferem."); return; }
            if (!App.criarAdmin(nomeField.text, loginField.text, senhaField.text))
                erro.text = App.ultimoErro();
        } else {
            if (!App.login(loginField.text, senhaField.text))
                erro.text = App.ultimoErro();
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: 380
        spacing: Theme.spacingLg

        // Marca (logo da distribuidora)
        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 260
            Layout.preferredHeight: 260
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 360
            sourceSize.height: 360
            smooth: true
            asynchronous: true
        }

        // Cartão
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: form.implicitHeight + 2 * Theme.spacingLg

            ColumnLayout {
                id: form
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingLg
                spacing: Theme.spacingMd

                Text {
                    text: tela.modoCriar ? qsTr("Criar administrador") : qsTr("Entrar")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                }
                Text {
                    visible: tela.modoCriar
                    text: qsTr("Primeiro acesso: defina o administrador do sistema.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                FormField {
                    visible: tela.modoCriar
                    label: qsTr("Nome")
                    Layout.fillWidth: true
                    AppTextField { id: nomeField; width: parent.width; placeholderText: qsTr("Seu nome") }
                }
                FormField {
                    label: qsTr("Login")
                    Layout.fillWidth: true
                    AppTextField { id: loginField; width: parent.width; placeholderText: qsTr("usuário") }
                }
                FormField {
                    label: qsTr("Senha")
                    Layout.fillWidth: true
                    AppTextField {
                        id: senhaField
                        width: parent.width
                        echoMode: TextInput.Password
                        onAccepted: if (!tela.modoCriar) tela.entrar()
                    }
                }
                FormField {
                    visible: tela.modoCriar
                    label: qsTr("Confirmar senha")
                    Layout.fillWidth: true
                    AppTextField {
                        id: confirmField
                        width: parent.width
                        echoMode: TextInput.Password
                        onAccepted: tela.entrar()
                    }
                }

                Label {
                    id: erro
                    visible: text.length > 0
                    color: Theme.danger
                    font.pixelSize: Theme.fontSm
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                AppButton {
                    kind: "accent"
                    text: tela.modoCriar ? qsTr("Criar e entrar") : qsTr("Entrar")
                    Layout.fillWidth: true
                    onClicked: tela.entrar()
                }
            }
        }
    }
}
