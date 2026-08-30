import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Janela principal: barra lateral + topbar + área de conteúdo trocável.
ApplicationWindow {
    id: janela
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: qsTr("Empório dos Amigos — Gestão")
    color: Theme.background

    // Paleta da aplicação ligada ao tema: faz os controles Fusion não estilizados
    // (ItemDelegate, ScrollBar, ToolButton, Menu, ToolTip…) seguirem a identidade
    // em vez da paleta do sistema (que deixava caixas cinzas escuras no claro).
    palette.window: Theme.background
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceAlt
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.brightText: "#FFFFFF"
    palette.light: Theme.surfaceAlt
    palette.midlight: Theme.surfaceAlt
    palette.mid: Theme.border
    palette.dark: Theme.borderStrong
    palette.shadow: Theme.border
    palette.highlight: Theme.primary
    palette.highlightedText: "#FFFFFF"
    palette.placeholderText: Theme.textMuted
    palette.toolTipBase: Theme.surface
    palette.toolTipText: Theme.text

    property string rotaAtual: "dashboard"

    // Título e subtítulo da topbar por rota (espelha o mockup).
    readonly property var cabecalhos: ({
        "dashboard":  { t: qsTr("Dashboard"),            s: qsTr("Visão geral da loja") },
        "pdv":        { t: qsTr("PDV — Frente de caixa"), s: qsTr("Venda em andamento") },
        "produtos":   { t: qsTr("Produtos"),             s: qsTr("Cadastro, embalagens e preços") },
        "estoque":    { t: qsTr("Estoque"),              s: qsTr("Quantidades, custo médio e alertas") },
        "compras":    { t: qsTr("Compras"),              s: qsTr("Fornecedores e entrada de mercadoria") },
        "clientes":   { t: qsTr("Clientes"),             s: qsTr("Cadastro e fiado") },
        "financeiro": { t: qsTr("Financeiro"),           s: qsTr("Contas a pagar e receber") },
        "relatorios": { t: qsTr("Relatórios"),           s: qsTr("Indicadores e desempenho") },
        "vendas":     { t: qsTr("Vendas"),               s: qsTr("Histórico e cancelamento") },
        "usuarios":   { t: qsTr("Usuários"),             s: qsTr("Perfis e permissões") },
        "backup":     { t: qsTr("Backup"),               s: qsTr("Cópias de segurança e restauração") }
    })
    readonly property var _cab: cabecalhos[rotaAtual] ? cabecalhos[rotaAtual] : cabecalhos["dashboard"]

    // Navegação pedida por uma tela (ex.: clicar em "Produtos em falta" no
    // Dashboard e cair no Estoque). Mantém a barra lateral em sincronia — sem
    // isto o item aceso continuaria sendo o antigo.
    function irPara(rota) {
        var cab = cabecalhos[rota];
        if (!cab)
            return;
        rotaAtual = rota;
        sidebar.rotaAtual = rota;
        conteudo.mostrar(rota, cab.t);
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: App.logado

        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: 248
            onNavegar: (rota, titulo) => {
                janela.rotaAtual = rota;
                conteudo.mostrar(rota, titulo);
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // --- Topbar ---
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 68
                color: Theme.surface

                Rectangle { // borda inferior
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingLg
                    anchors.rightMargin: Theme.spacingLg
                    spacing: Theme.spacingMd

                    Column {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            text: janela._cab.t
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.fontXl
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: janela._cab.s
                            color: Theme.textMuted
                            font.family: Theme.fontBase
                            font.pixelSize: Theme.fontSm
                        }
                    }

                    // Status do caixa
                    Rectangle {
                        visible: App.caixaAberto
                        implicitHeight: 30
                        implicitWidth: pillRow.implicitWidth + 24
                        radius: 15
                        color: Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 0.15)
                        Row {
                            id: pillRow
                            anchors.centerIn: parent
                            spacing: Theme.spacingSm
                            Rectangle {
                                width: 7; height: 7; radius: 3.5
                                color: Theme.success
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: qsTr("Caixa aberto")
                                color: Theme.success
                                font.family: Theme.fontBase
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    AppButton {
                        kind: "ghost"
                        text: Theme.dark ? qsTr("☀ Claro") : qsTr("☾ Escuro")
                        onClicked: Theme.dark = !Theme.dark
                    }

                    Text {
                        text: App.usuarioAtual.nome !== undefined ? App.usuarioAtual.nome : ""
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSm
                    }
                    AppButton {
                        kind: "ghost"
                        text: qsTr("Sair")
                        onClicked: App.logout()
                    }
                }
            }

            // --- Conteúdo ---
            StackView {
                id: conteudo
                Layout.fillWidth: true
                Layout.fillHeight: true
                initialItem: dashboardComp
                clip: true

                // Fade rápido e limpo (o slide padrão do StackView ficava ruim).
                // O novo entra por cima; o antigo fica opaco por baixo até o fim
                // (PauseAnimation) para não piscar o fundo.
                replaceEnter: Transition {
                    NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 130; easing.type: Easing.OutCubic }
                }
                replaceExit: Transition {
                    PauseAnimation { duration: 130 }
                }

                function mostrar(rota, titulo) {
                    if (rota === "dashboard")
                        conteudo.replace(dashboardComp);
                    else if (rota === "produtos")
                        conteudo.replace(produtosComp);
                    else if (rota === "estoque")
                        conteudo.replace(estoqueComp);
                    else if (rota === "pdv")
                        conteudo.replace(pdvComp);
                    else if (rota === "usuarios")
                        conteudo.replace(usuariosComp);
                    else if (rota === "compras")
                        conteudo.replace(comprasComp);
                    else if (rota === "clientes")
                        conteudo.replace(clientesComp);
                    else if (rota === "financeiro")
                        conteudo.replace(financeiroComp);
                    else if (rota === "relatorios")
                        conteudo.replace(relatoriosComp);
                    else if (rota === "vendas")
                        conteudo.replace(vendasComp);
                    else if (rota === "backup")
                        conteudo.replace(backupComp);
                    else
                        conteudo.replace(placeholderComp, { titulo: titulo });
                }
            }
        }
    }

    // Portão de acesso: cobre tudo até o login.
    LoginScreen {
        anchors.fill: parent
        visible: !App.logado
    }


    Component {
        id: dashboardComp
        DashboardScreen {
            onNavegar: (rota) => janela.irPara(rota)
        }
    }
    Component {
        id: produtosComp
        ProdutosScreen {}
    }
    Component {
        id: estoqueComp
        EstoqueScreen {}
    }
    Component {
        id: pdvComp
        PdvScreen {}
    }
    Component {
        id: usuariosComp
        UsuariosScreen {}
    }
    Component {
        id: comprasComp
        ComprasScreen {}
    }
    Component {
        id: clientesComp
        ClientesScreen {}
    }
    Component {
        id: financeiroComp
        FinanceiroScreen {}
    }
    Component {
        id: relatoriosComp
        RelatoriosScreen {}
    }
    Component {
        id: vendasComp
        VendasScreen {}
    }
    Component {
        id: backupComp
        BackupScreen {}
    }
    Component {
        id: placeholderComp
        PlaceholderScreen {}
    }
}
