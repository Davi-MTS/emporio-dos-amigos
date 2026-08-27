import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// "Ver no celular": gera um relatório HTML numa pasta do OneDrive (privada),
// atualizado ao fechar o caixa e por este botão. Só Administrador.
Rectangle {
    id: tela
    color: Theme.background

    property var status: ({})

    Component.onCompleted: carregar()
    function carregar() { status = App.statusRelatorioCelular(); }
    function fmtData(iso) {
        if (!iso || iso.length === 0) return "—";
        return Qt.formatDateTime(new Date(iso), "dd/MM/yyyy HH:mm");
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Status + ação
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: col.implicitHeight + 2 * Theme.spacingLg
            ColumnLayout {
                id: col
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Relatório do celular")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.fontXl
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: tela.status.existe
                                  ? qsTr("Atualizado em ") + tela.fmtData(tela.status.atualizadoEm)
                                  : qsTr("Ainda não gerado.")
                            color: tela.status.existe ? Theme.success : Theme.warning
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                        }
                    }
                    AppButton {
                        kind: "accent"
                        text: qsTr("Atualizar agora")
                        onClicked: {
                            var r = App.gerarRelatorioCelular();
                            if (r.ok) { aviso.mostrar(qsTr("Relatório atualizado."), false); tela.carregar(); }
                            else aviso.mostrar(r.erro, true);
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: qsTr("Pasta: ") + (tela.status.pasta || "")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("O relatório é atualizado automaticamente ao fechar o caixa. Contém o resumo (hoje/7/30 dias), formas de pagamento, mais vendidos, estoque e fiado.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }
                Label {
                    id: aviso
                    property bool erro: false
                    function mostrar(t, e) { erro = e; text = t; visible = true; }
                    visible: false
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: erro ? Theme.danger : Theme.success
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                }
            }
        }

        // Instruções
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingSm
                Text {
                    text: qsTr("Como ver no celular")
                    color: Theme.text
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontMd
                    text: qsTr("1. Instale o aplicativo OneDrive no celular e entre na mesma conta Microsoft deste computador.\n\n"
                             + "2. Abra a pasta “Empório dos Amigos › Relatório” e toque no arquivo relatorio.html.\n\n"
                             + "3. Se pedir, escolha abrir no navegador. Marque como favorito para achar rápido depois.\n\n"
                             + "O relatório é privado da sua conta OneDrive e é somente leitura.")
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
