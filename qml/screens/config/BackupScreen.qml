import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Distribuidora

// Backup e restauração (só Administrador). Backup manual + automático (ao fechar
// caixa), com retenção de 5 cópias, e restauração agendada para o próximo início.
Rectangle {
    id: tela
    color: Theme.background

    property var status: ({})
    property var relatorio: ({})
    property var log: ({})
    property var linhasLog: []

    ListModel { id: backupsModel }

    Component.onCompleted: carregar()
    function carregar() {
        status = App.statusBackup();
        relatorio = App.statusRelatorioCelular();
        var tg = App.configTelegram();
        tgToken.text = tg.token || "";
        tgChat.text = tg.chatId || "";
        tgAtivo.checked = tg.ativo === true;
        tgBackup.checked = tg.enviaBackup === true;
        carregarLog();
        backupsModel.clear();
        var l = App.backupsDisponiveis();
        for (var i = 0; i < l.length; i++)
            backupsModel.append(l[i]);
    }
    function carregarLog() {
        log = App.statusLog();
        linhasLog = App.ultimasLinhasLog(80);
    }
    function fmtData(iso) {
        if (!iso || iso.length === 0) return "—";
        return Qt.formatDateTime(new Date(iso), "dd/MM/yyyy HH:mm");
    }
    function fmtTam(b) {
        if (b >= 1048576) return (b / 1048576).toFixed(1) + " MB";
        if (b >= 1024) return Math.round(b / 1024) + " KB";
        return b + " B";
    }

    // A tela tem muito conteúdo (backup, Telegram, registro e a lista de cópias):
    // sem rolagem, o que passa da altura da janela ficava inacessível.
    ScrollView {
        id: rolagem
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        width: rolagem.availableWidth
        spacing: Theme.spacingMd

        // ---- Status + ações ----
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: statusCol.implicitHeight + 2 * Theme.spacingLg

            ColumnLayout {
                id: statusCol
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
                            text: qsTr("Backup do sistema")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.fontXl
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: (tela.status.ultimoCriadoEm && tela.status.ultimoCriadoEm.length > 0)
                                  ? qsTr("Último backup: ") + tela.fmtData(tela.status.ultimoCriadoEm)
                                    + "  ·  " + (tela.status.ultimoResumo || "")
                                  : qsTr("Nenhum backup ainda.")
                            color: (tela.status.ultimoCriadoEm && tela.status.ultimoCriadoEm.length > 0)
                                   ? Theme.success : Theme.warning
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                        }
                    }
                    AppButton {
                        kind: "accent"
                        text: qsTr("Fazer backup agora")
                        onClicked: {
                            var r = App.fazerBackup();
                            if (r.ok) { aviso.mostrar(qsTr("Backup criado — ") + r.resumo, false); tela.carregar(); }
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
                    text: qsTr("Ao fechar o caixa (e neste botão) é feito o backup automaticamente, mantendo as 5 cópias mais recentes.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    elide: Text.ElideMiddle
                    visible: tela.relatorio.existe === true
                    text: qsTr("Relatório do celular: atualizado em ")
                          + tela.fmtData(tela.relatorio.atualizadoEm) + "\n" + (tela.relatorio.pasta || "")
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

        // ---- Aviso no celular (Telegram) ----
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: tgCol.implicitHeight + 2 * Theme.spacingLg

            Connections {
                target: App
                function onTelegramChatDescoberto(chatId, nome) {
                    tgChat.text = chatId;
                    tgAviso.erro = false;
                    tgAviso.text = qsTr("Encontrei: ") + (nome && nome.length ? nome : chatId)
                                 + qsTr(" — agora clique em Salvar.");
                    tgAviso.visible = true;
                }
                function onTelegramResultado(ok, mensagem) {
                    tgAviso.erro = !ok;
                    tgAviso.text = ok ? qsTr("Enviado! Confira o celular.") : mensagem;
                    tgAviso.visible = true;
                }
            }

            ColumnLayout {
                id: tgCol
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingMd

                Text {
                    text: qsTr("Aviso no celular (Telegram)")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Ao fechar o caixa, o resumo do dia é enviado como notificação para o celular dos donos, com o relatório completo anexado. Funciona de qualquer lugar, sem depender de nuvem.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }

                FormField {
                    label: qsTr("Token do bot")
                    Layout.fillWidth: true
                    AppTextField {
                        id: tgToken
                        width: parent.width
                        placeholderText: qsTr("cole aqui o token que o @BotFather enviou")
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    FormField {
                        label: qsTr("Chat / grupo")
                        Layout.fillWidth: true
                        AppTextField {
                            id: tgChat
                            width: parent.width
                            placeholderText: qsTr("clique em Descobrir →")
                        }
                    }
                    AppButton {
                        kind: "default"
                        text: qsTr("Descobrir")
                        Layout.alignment: Qt.AlignBottom
                        onClicked: {
                            tgAviso.erro = false;
                            tgAviso.text = qsTr("Procurando a conversa…");
                            tgAviso.visible = true;
                            App.descobrirChatTelegram(tgToken.text);
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Para descobrir: adicione o SEU bot ao grupo, mande lá uma mensagem começando com barra (ex.: /oi) e clique em “Descobrir”. O bot só enxerga mensagens com / — as comuns ficam ocultas para ele.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }

                ToggleButton {
                    id: tgAtivo
                    text: qsTr("Enviar automaticamente ao fechar o caixa")
                }
                ToggleButton {
                    id: tgBackup
                    text: qsTr("Enviar também a cópia de segurança")
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: tgBackup.checked
                          ? qsTr("A cópia do banco vai junto, então os dados deixam de existir só neste computador. Se o HD falhar ou o PC for roubado, dá para recuperar pelo Telegram.")
                          : qsTr("⚠️ Sem isto, o backup fica SÓ neste computador. Se o HD falhar ou o PC for roubado, os dados se perdem.")
                    color: tgBackup.checked ? Theme.textMuted : Theme.warning
                    font.pixelSize: Theme.fontXs
                }

                Label {
                    id: tgAviso
                    property bool erro: false
                    visible: false
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: erro ? Theme.danger : Theme.success
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    AppButton {
                        kind: "accent"
                        text: qsTr("Salvar")
                        onClicked: {
                            App.salvarConfigTelegram(tgToken.text, tgChat.text, tgAtivo.checked, tgBackup.checked);
                            tgAviso.erro = false;
                            tgAviso.text = qsTr("Configuração salva.");
                            tgAviso.visible = true;
                        }
                    }
                    AppButton {
                        kind: "default"
                        text: qsTr("Enviar teste agora")
                        onClicked: {
                            App.salvarConfigTelegram(tgToken.text, tgChat.text, tgAtivo.checked, tgBackup.checked);
                            tgAviso.erro = false;
                            tgAviso.text = qsTr("Enviando…");
                            tgAviso.visible = true;
                            App.testarTelegram();
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ---- Registro do sistema (log) ----
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            implicitHeight: logCol.implicitHeight + 2 * Theme.spacingLg

            ColumnLayout {
                id: logCol
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
                spacing: Theme.spacingSm

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Registro do sistema")
                            color: Theme.text
                            font.family: Theme.fontDisplay
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Guarda o que o sistema fez e os erros que aconteceram. É o que permite descobrir a causa se algo falhar ou fechar sozinho.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                        }
                    }
                    AppButton {
                        kind: "default"
                        text: qsTr("Abrir pasta")
                        onClicked: Qt.openUrlExternally(tela.log.url || "")
                    }
                    AppButton {
                        kind: "ghost"
                        text: qsTr("↻")
                        implicitWidth: 40
                        onClicked: tela.carregarLog()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    radius: Theme.radiusSm
                    color: Theme.surfaceAlt
                    border.color: Theme.border
                    clip: true
                    ListView {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        clip: true
                        model: tela.linhasLog
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Text {
                            required property var modelData
                            width: ListView.view.width
                            text: modelData
                            wrapMode: Text.Wrap
                            font.family: "Consolas"
                            font.pixelSize: Theme.fontXs
                            color: modelData.indexOf("[ERRO]") >= 0 || modelData.indexOf("[FATAL]") >= 0
                                   ? Theme.danger
                                   : (modelData.indexOf("[AVISO]") >= 0 ? Theme.warning : Theme.textMuted)
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: tela.linhasLog.length === 0
                            text: qsTr("Sem registros ainda.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSm
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: (tela.log.arquivo || "") + "  ·  " + tela.fmtTam(tela.log.tamanho || 0)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("Cópias disponíveis")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.6
            }
            // A lista só enxerga a pasta de backups. A cópia que veio do
            // Telegram (ou de um pendrive) está em Downloads — sem isto não
            // havia como restaurá-la, que é justo o caso de HD queimado.
            AppButton {
                kind: "ghost"
                text: qsTr("Restaurar de um arquivo…")
                onClicked: escolherBackupDialog.open()
            }
        }

        FileDialog {
            id: escolherBackupDialog
            title: qsTr("Escolha o arquivo de backup")
            nameFilters: [qsTr("Backup do sistema (*.db)"), qsTr("Todos os arquivos (*)")]
            onAccepted: {
                var caminho = escolherBackupDialog.selectedFile.toString()
                                  .replace(/^file:\/{2,3}/, "");
                caminho = decodeURIComponent(caminho);
                var c = App.conferirArquivoBackup(caminho);
                if (!c.ok) {
                    aviso.mostrar(c.erro, true);
                    return;
                }
                restaurarDialog.abrir(caminho, tela.fmtData(c.criadoEm), c.resumo);
            }
        }

        // ---- Lista de backups ----
        Rectangle {
            Layout.fillWidth: true
            // Altura própria: dentro de um ScrollView não existe "resto da tela".
            Layout.preferredHeight: Math.max(160, Math.min(lista.count, 6) * 58 + 16)
            radius: Theme.radius
            color: Theme.surface
            border.color: Theme.border
            clip: true

            ListView {
                id: lista
                anchors.fill: parent
                clip: true
                model: backupsModel
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    id: row
                    required property int index
                    required property string caminho
                    required property string criadoEm
                    required property var tamanho
                    required property string resumo
                    width: ListView.view.width
                    height: 58
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingMd
                        spacing: Theme.spacingMd
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: tela.fmtData(row.criadoEm)
                                color: Theme.text
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: row.resumo + "  ·  " + tela.fmtTam(row.tamanho)
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontXs
                            }
                        }
                        AppButton {
                            kind: "default"
                            text: qsTr("Restaurar")
                            onClicked: restaurarDialog.abrir(row.caminho, tela.fmtData(row.criadoEm), row.resumo)
                        }
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                }
                Label {
                    anchors.centerIn: parent
                    visible: lista.count === 0
                    text: qsTr("Nenhum backup ainda. Clique em “Fazer backup agora”.")
                    color: Theme.textMuted
                }
            }
        }
    }
    }

    // Confirmar restauração
    AppDialog {
        id: restaurarDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 500
        padding: Theme.spacingLg
        property string caminho: ""
        property string quando: ""
        property string resumo: ""
        function abrir(c, q, r) { caminho = c; quando = q; resumo = r; open(); }
        title: qsTr("Restaurar backup")
        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Isto vai substituir TODOS os dados atuais pela cópia de ") + restaurarDialog.quando + "."
                color: Theme.text
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                text: restaurarDialog.resumo
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Uma cópia de segurança do estado atual é criada antes, por garantia. Para concluir, o aplicativo precisa ser fechado e aberto de novo.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    kind: "accent"
                    text: qsTr("Restaurar")
                    onClicked: {
                        var r = App.agendarRestauracao(restaurarDialog.caminho);
                        restaurarDialog.close();
                        if (r.ok) prontoDialog.open();
                        else aviso.mostrar(r.erro, true);
                    }
                }
                AppButton { kind: "default"; text: qsTr("Cancelar"); onClicked: restaurarDialog.close() }
                Item { Layout.fillWidth: true }
            }
        }
    }

    // Restauração agendada
    AppDialog {
        id: prontoDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: 460
        padding: Theme.spacingLg
        title: qsTr("Restauração agendada")
        standardButtons: Dialog.Ok
        contentItem: ColumnLayout {
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Feche e abra o sistema novamente para concluir. Ao reabrir, os dados estarão como no backup escolhido.")
                color: Theme.text
                font.pixelSize: Theme.fontMd
            }
        }
    }
}
