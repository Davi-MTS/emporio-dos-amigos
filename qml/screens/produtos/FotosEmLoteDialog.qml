import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.folderlistmodel
import Distribuidora

// Fila de fotos: entra um monte de arquivo de uma vez e você vai dizendo de
// quem é cada um.
//
// O gargalo de pôr foto em produto nunca foi escolher o arquivo — é dizer a
// qual produto ele pertence. Antes disso existir, cada foto custava: abrir o
// produto no cadastro, achar o botão, navegar até a pasta, escolher, fechar.
// Para os ~200 produtos da loja, era inviável.
//
// Aqui a foto aparece grande e você identifica pelo próprio rótulo: bipa o
// código de barras (o leitor digita e dá Enter sozinho) ou digita duas letras
// do nome. Atribuiu, já pula para a próxima com o campo limpo e focado.
AppDialog {
    id: dlg

    title: qsTr("Fotos em lote")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 2 * Theme.spacingLg : 1000, 1040)
    height: alturaMaxima
    closePolicy: Popup.NoAutoClose   // fechar sem querer no meio da fila irrita

    // Fila de arquivos (URLs "file:///..."), na ordem em que entraram.
    property var fila: []
    property int indice: 0
    property int atribuidas: 0
    property int puladas: 0
    property string erro: ""

    // Para o Desfazer. `ultimoSubstituiu` existe porque desfazer uma
    // SUBSTITUIÇÃO significaria devolver a foto antiga, e essa a gente não
    // guardou — então nesse caso o botão fica desligado em vez de mentir.
    property int ultimoProdutoId: 0
    property bool ultimoSubstituiu: false

    readonly property bool temFila: indice < fila.length
    readonly property string atual: temFila ? fila[indice] : ""

    signal precisaRecarregar()

    function reiniciar() {
        fila = [];
        indice = 0;
        atribuidas = 0;
        puladas = 0;
        erro = "";
        candidatos = [];
        ultimoProdutoId = 0;
        ultimoSubstituiu = false;
        verFaltantes = false;
        buscaField.text = "";
    }

    function comecar(urls) {
        var novas = [];
        for (var i = 0; i < urls.length; i++) {
            var u = "" + urls[i];
            if (_ehImagem(u))
                novas.push(u);
        }
        if (novas.length === 0) {
            erro = qsTr("Nenhuma imagem nos arquivos escolhidos.");
            return;
        }
        // Somar à fila em vez de trocar: dá para largar uma pasta, lembrar de
        // outra e largar também, sem perder o que já foi feito.
        fila = fila.concat(novas);
        erro = "";
        _focar();
    }

    function _ehImagem(u) {
        return /\.(png|jpe?g|bmp|gif)$/i.test(("" + u).split("?")[0]);
    }

    function _caminho(u) {
        return decodeURIComponent(("" + u).replace(/^file:\/{2,3}/, ""));
    }

    function _focar() { buscaField.forceActiveFocus(); }

    function proxima() {
        indice = indice + 1;
        buscaField.text = "";
        candidatos = [];
        erro = "";
        _focar();
    }

    function pular() {
        if (!temFila)
            return;
        puladas = puladas + 1;
        ultimoProdutoId = 0;   // pular não é atribuição: nada a desfazer
        proxima();
    }

    function atribuir(produtoId, jaTemFoto) {
        if (!temFila || produtoId <= 0)
            return;
        if (jaTemFoto) {
            confirmaTroca.produtoId = produtoId;
            confirmaTroca.open();
            return;
        }
        var r = App.definirFotoProduto(produtoId, _caminho(atual));
        if (!r.ok) {
            erro = r.erro;
            return;
        }
        atribuidas = atribuidas + 1;
        ultimoProdutoId = produtoId;
        ultimoSubstituiu = jaTemFoto;
        dlg.precisaRecarregar();
        proxima();
    }

    function desfazer() {
        if (ultimoProdutoId <= 0 || ultimoSubstituiu)
            return;
        App.removerFotoProduto(ultimoProdutoId);
        ultimoProdutoId = 0;
        atribuidas = Math.max(0, atribuidas - 1);
        indice = Math.max(0, indice - 1);
        buscaField.text = "";
        candidatos = [];
        dlg.precisaRecarregar();
        _focar();
    }

    // Candidatos da busca por nome. Vêm prontos do backend com `temFoto`.
    property var candidatos: []

    function buscar(termo) {
        candidatos = termo.trim().length >= 2 ? App.buscarProdutosPorNome(termo) : [];
    }

    // Enter: primeiro tenta código de barras exato (o leitor manda os dígitos e
    // dá Enter); não sendo código, usa o primeiro nome da lista.
    function confirmarBusca() {
        var t = buscaField.text.trim();
        if (t.length === 0)
            return;
        var porCodigo = App.buscarProdutoPorCodigo(t);
        if (porCodigo.encontrado) {
            atribuir(porCodigo.produtoId, porCodigo.temFoto === true);
            return;
        }
        if (candidatos.length > 0)
            atribuir(candidatos[0].produtoId, candidatos[0].temFoto === true);
    }

    // ------------------------------------------------------------- conteúdo
    contentItem: Item {

        // Arrastar e soltar cobre a tela inteira; fica ATRÁS do conteúdo para
        // não roubar clique nenhum (DropArea só trata arrasto, mas a ordem
        // deixa isso explícito).
        DropArea {
            anchors.fill: parent
            onDropped: (drop) => {
                if (drop.hasUrls)
                    dlg.comecar(drop.urls);
            }
        }

        ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingMd

        // ===================== fila vazia: como trazer as fotos =====================
        ColumnLayout {
            visible: dlg.fila.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Traga as fotos do celular para uma pasta do computador e escolha "
                           + "a pasta inteira de uma vez. Você também pode arrastar os arquivos "
                           + "para cá.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontMd
            }

            Flow {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.spacingSm
                AppButton {
                    kind: "accent"
                    text: qsTr("Escolher pasta…")
                    onClicked: pastaDialog.open()
                }
                AppButton {
                    kind: "default"
                    text: qsTr("Escolher fotos…")
                    onClicked: arquivosDialog.open()
                }
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Formatos aceitos: JPG, PNG, BMP e GIF. Foto de iPhone em HEIC não "
                           + "abre — no celular, Ajustes → Câmera → Formatos → Mais Compatível.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
            }

            Item { Layout.fillHeight: true }
        }

        // ===================== a fila andando =====================
        RowLayout {
            visible: dlg.fila.length > 0 && dlg.temFila
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingLg

            // ---------------------------------------------------- a foto
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                radius: Theme.radius
                color: Theme.surfaceAlt
                border.color: Theme.border
                border.width: 1
                clip: true

                Image {
                    id: previa
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    source: dlg.atual
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                }

                // Arquivo que o Qt não abre (HEIC é o caso comum) não pode
                // virar tela em branco sem explicação.
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 2 * Theme.spacingLg
                    visible: previa.status === Image.Error
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Não consigo abrir esta imagem.\nPule para a próxima.")
                    color: Theme.danger
                    font.pixelSize: Theme.fontMd
                }
            }

            // ---------------------------------------------------- painel
            ColumnLayout {
                Layout.preferredWidth: 300
                Layout.maximumWidth: 340
                Layout.fillHeight: true
                spacing: Theme.spacingSm

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("De quem é esta foto?")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Bipe o código de barras ou digite o nome.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontXs
                }

                AppTextField {
                    id: buscaField
                    objectName: "buscaFoto"
                    Layout.fillWidth: true
                    placeholderText: qsTr("código ou nome…")
                    onTextChanged: dlg.buscar(text)
                    onAccepted: dlg.confirmarBusca()
                }

                // Candidatos. Clicar atribui direto — é o caminho de quem
                // digita o nome em vez de bipar.
                ListView {
                    id: listaCandidatos
                    objectName: "candidatosFoto"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    clip: true
                    model: dlg.candidatos
                    spacing: 2

                    delegate: ItemDelegate {
                        id: cand
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        height: 42
                        onClicked: dlg.atribuir(cand.modelData.produtoId,
                                                cand.modelData.temFoto === true)

                        contentItem: RowLayout {
                            spacing: Theme.spacingSm
                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                elide: Text.ElideRight
                                text: cand.modelData.nome
                                color: Theme.text
                                font.pixelSize: Theme.fontSm
                            }
                            // Sem este aviso dá para sobrescrever a foto certa
                            // de um produto sem perceber.
                            Rectangle {
                                visible: cand.modelData.temFoto === true
                                implicitWidth: jaTem.implicitWidth + 12
                                implicitHeight: 18
                                radius: 5
                                color: Theme.accentSoft
                                Text {
                                    id: jaTem
                                    anchors.centerIn: parent
                                    text: qsTr("já tem foto")
                                    color: Theme.warning
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: dlg.erro.length > 0
                    wrapMode: Text.WordWrap
                    text: dlg.erro
                    color: Theme.danger
                    font.pixelSize: Theme.fontXs
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    AppButton {
                        kind: "default"
                        text: qsTr("Pular")
                        onClicked: dlg.pular()
                    }
                    AppButton {
                        objectName: "desfazerFoto"
                        kind: "ghost"
                        text: qsTr("↩ Desfazer")
                        enabled: dlg.ultimoProdutoId > 0 && !dlg.ultimoSubstituiu
                        onClicked: dlg.desfazer()
                    }
                }
            }
        }

        // ===================== acabou a fila =====================
        ColumnLayout {
            visible: dlg.fila.length > 0 && !dlg.temFila
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Fila terminada")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.fontXl
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: {
                    var faltam = App.contarProdutosSemFoto();
                    var base = qsTr("%1 fotos atribuídas, %2 puladas.")
                                 .arg(dlg.atribuidas).arg(dlg.puladas);
                    return faltam > 0
                           ? base + " " + qsTr("Ainda há %1 produtos sem foto.").arg(faltam)
                           : base + " " + qsTr("Todos os produtos já têm foto.");
                }
                color: Theme.textMuted
                font.pixelSize: Theme.fontMd
            }
            Flow {
                Layout.alignment: Qt.AlignHCenter
                spacing: Theme.spacingSm
                AppButton {
                    kind: "default"
                    text: qsTr("Escolher mais fotos…")
                    onClicked: arquivosDialog.open()
                }
                AppButton {
                    kind: "accent"
                    text: qsTr("Ver quem ainda falta")
                    onClicked: { dlg.verFaltantes = true; dlg.close(); }
                }
            }
            Item { Layout.fillHeight: true }
        }

        // ===================== rodapé =====================
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
                text: dlg.fila.length === 0
                      ? qsTr("Nenhuma foto na fila")
                      : qsTr("%1 de %2  ·  %3 atribuídas  ·  %4 puladas")
                          .arg(Math.min(dlg.indice + 1, dlg.fila.length))
                          .arg(dlg.fila.length).arg(dlg.atribuidas).arg(dlg.puladas)
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            AppButton {
                kind: "ghost"
                text: qsTr("Fechar")
                onClicked: dlg.close()
            }
        }
        }
    }

    // Sai como `true` quando o dono pediu para ver os que ficaram sem foto.
    property bool verFaltantes: false

    onOpened: { verFaltantes = false; _focar(); }

    // ------------------------------------------------------- escolher arquivos
    FileDialog {
        id: arquivosDialog
        title: qsTr("Escolha as fotos")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Imagens (*.png *.jpg *.jpeg *.bmp *.gif)")]
        onAccepted: dlg.comecar(arquivosDialog.selectedFiles)
    }

    // ------------------------------------------------------- escolher pasta
    FolderDialog {
        id: pastaDialog
        title: qsTr("Escolha a pasta com as fotos")
        onAccepted: pastaModel.folder = pastaDialog.selectedFolder
    }

    FolderListModel {
        id: pastaModel
        showDirs: false
        showDotAndDotDot: false
        sortField: FolderListModel.Name
        nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"]
        onStatusChanged: {
            if (status !== FolderListModel.Ready || count === 0)
                return;
            var urls = [];
            for (var i = 0; i < count; i++)
                urls.push(get(i, "fileUrl"));
            dlg.comecar(urls);
        }
    }

    // ------------------------------------------------------- confirmar troca
    AppDialog {
        id: confirmaTroca
        title: qsTr("Substituir a foto?")
        anchors.centerIn: Overlay.overlay
        modal: true
        property int produtoId: 0

        contentItem: ColumnLayout {
            spacing: Theme.spacingMd
            Text {
                Layout.fillWidth: true
                Layout.maximumWidth: 380
                wrapMode: Text.WordWrap
                text: qsTr("Este produto já tem foto. A foto atual será perdida — o Desfazer "
                           + "não devolve a antiga.")
                color: Theme.text
                font.pixelSize: Theme.fontSm
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                Item { Layout.fillWidth: true }
                AppButton {
                    kind: "ghost"
                    text: qsTr("Cancelar")
                    onClicked: confirmaTroca.close()
                }
                AppButton {
                    kind: "accent"
                    text: qsTr("Substituir")
                    onClicked: {
                        var id = confirmaTroca.produtoId;
                        confirmaTroca.close();
                        // Segunda passada: agora `jaTemFoto` entra como false
                        // para não abrir a confirmação de novo. O flag de
                        // substituição é gravado à parte.
                        var r = App.definirFotoProduto(id, dlg._caminho(dlg.atual));
                        if (!r.ok) {
                            dlg.erro = r.erro;
                            return;
                        }
                        dlg.atribuidas = dlg.atribuidas + 1;
                        dlg.ultimoProdutoId = id;
                        dlg.ultimoSubstituiu = true;
                        dlg.precisaRecarregar();
                        dlg.proxima();
                    }
                }
            }
        }
    }
}
