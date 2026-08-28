import QtQuick
import QtQuick.Layouts
import Distribuidora

// Barra lateral de navegação (marca + seções + rodapé), igual ao mockup.
// Emite `navegar` com a rota e o título da tela.
Rectangle {
    id: sidebar

    signal navegar(string rota, string titulo)
    property string rotaAtual: "dashboard"

    color: Theme.sidebar

    readonly property var operacao: [
        { rota: "dashboard", titulo: "Dashboard" },
        { rota: "pdv",       titulo: "PDV" },
        { rota: "produtos",  titulo: "Produtos" },
        { rota: "estoque",   titulo: "Estoque" },
        { rota: "vendas",    titulo: "Vendas" }
    ]
    readonly property var retaguarda: [
        { rota: "compras",    titulo: "Compras",     perm: "ve_financeiro" },
        { rota: "clientes",   titulo: "Clientes" },
        { rota: "financeiro", titulo: "Financeiro",  perm: "ve_financeiro" },
        { rota: "relatorios", titulo: "Relatórios" },
        { rota: "usuarios",   titulo: "Usuários",       perm: "gerencia_usuarios" },
        { rota: "backup",     titulo: "Backup",         perm: "gerencia_usuarios" }
    ]

    function _selecionar(item) {
        rotaAtual = item.rota;
        navegar(item.rota, item.titulo);
    }
    function temPerm(chave) {
        var p = (App.usuarioAtual && App.usuarioAtual.permissoes) ? App.usuarioAtual.permissoes : ({});
        return p.tudo === true || p[chave] === true;
    }
    function iniciais(nome) {
        if (!nome) return "?";
        var partes = nome.trim().split(/\s+/);
        var s = partes[0].charAt(0);
        if (partes.length > 1) s += partes[partes.length - 1].charAt(0);
        return s.toUpperCase();
    }

    // Rótulo de seção reutilizável.
    component SectionLabel: Text {
        color: Theme.textOnDarkMuted
        font.family: Theme.fontBase
        font.pixelSize: Theme.fontXs
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        opacity: 0.75
        Layout.leftMargin: Theme.spacingSm
        Layout.topMargin: Theme.spacingSm
        Layout.bottomMargin: 2
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingSm
        anchors.topMargin: Theme.spacingMd
        spacing: 3

        // --- Marca (logo da distribuidora) ---
        Image {
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            Layout.leftMargin: Theme.spacingSm
            Layout.topMargin: 2
            Layout.bottomMargin: Theme.spacingMd
            source: "qrc:/images/logo.png"
            fillMode: Image.PreserveAspectFit
            horizontalAlignment: Image.AlignLeft
            sourceSize.height: 200
            smooth: true
            asynchronous: true
        }

        SectionLabel { text: qsTr("OPERAÇÃO") }
        Repeater {
            model: sidebar.operacao
            delegate: NavButton {
                required property var modelData
                Layout.fillWidth: true
                text: modelData.titulo
                icone: modelData.rota
                ativo: sidebar.rotaAtual === modelData.rota
                onClicked: sidebar._selecionar(modelData)
            }
        }

        SectionLabel { text: qsTr("RETAGUARDA") }
        Repeater {
            model: sidebar.retaguarda
            delegate: NavButton {
                required property var modelData
                Layout.fillWidth: true
                text: modelData.titulo
                icone: modelData.rota
                ativo: sidebar.rotaAtual === modelData.rota
                visible: !modelData.perm || sidebar.temPerm(modelData.perm)
                onClicked: sidebar._selecionar(modelData)
            }
        }

        Item { Layout.fillHeight: true }

        // --- Rodapé (usuário) ---
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingSm
            spacing: Theme.spacingSm

            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 15
                color: Theme.sidebarActive
                Text {
                    anchors.centerIn: parent
                    text: sidebar.iniciais(App.usuarioAtual.nome)
                    color: Theme.textOnDark
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                }
            }
            Column {
                spacing: 0
                Text {
                    text: App.usuarioAtual.nome !== undefined ? App.usuarioAtual.nome : ""
                    color: Theme.textOnDark
                    font.family: Theme.fontBase
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                }
                Text {
                    text: App.usuarioAtual.perfil !== undefined ? App.usuarioAtual.perfil : ""
                    color: Theme.textOnDarkMuted
                    font.family: Theme.fontBase
                    font.pixelSize: Theme.fontXs
                }
            }
        }
    }
}
