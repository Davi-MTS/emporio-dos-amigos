import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Distribuidora

// Calendário para escolher um dia com o mouse, em vez de digitar dd/mm/aaaa.
//
// Uso:
//   CalendarioPopup { id: cal; maximo: "2026-09-15"; onEscolhido: (iso) => ... }
//   cal.abrirEm("2026-09-13")
//
// DATAS SEMPRE POR INTEIROS. O `date` que o MonthGrid entrega pode vir em UTC:
// formatado em hora local (Goiânia, UTC−3) vira as 21h do DIA ANTERIOR, e o
// relatório abriria o dia errado sem ninguém perceber. Por isso a data é montada
// com dia/mês/ano do próprio delegate, sem conversão de fuso.
Popup {
    id: cal

    // Dia marcado ("yyyy-MM-dd").
    property string selecionado: ""
    // Último dia que pode ser escolhido ("yyyy-MM-dd"); vazio = sem limite.
    // Nos relatórios é hoje: dia futuro não tem venda.
    property string maximo: ""

    signal escolhido(string iso)

    property int mes: new Date().getMonth()      // 0..11, como o JavaScript
    property int ano: new Date().getFullYear()

    readonly property var localBr: Qt.locale("pt_BR")
    readonly property int alturaDia: 36

    function _iso(a, m, d) {
        return a + "-" + (m + 1 < 10 ? "0" : "") + (m + 1) + "-" + (d < 10 ? "0" : "") + d;
    }
    function _permitido(iso) {
        return maximo.length === 0 || iso <= maximo;   // ISO compara como texto
    }

    function abrirEm(iso) {
        var m = ("" + iso).match(/^(\d{4})-(\d{2})-\d{2}$/);
        if (m) {
            ano = parseInt(m[1]);
            mes = parseInt(m[2]) - 1;
        }
        selecionado = iso;
        open();
    }

    function mesAnterior() {
        if (mes === 0) { mes = 11; ano = ano - 1; } else { mes = mes - 1; }
    }
    function proximoMes() {
        if (!podeAvancar) return;
        if (mes === 11) { mes = 0; ano = ano + 1; } else { mes = mes + 1; }
    }
    // Não deixa ir para um mês inteiro no futuro.
    readonly property bool podeAvancar: maximo.length === 0
                                        || _iso(mes === 11 ? ano + 1 : ano, (mes + 1) % 12, 1) <= maximo

    // Escolha de um dia. Dia além do máximo é recusado aqui também, e não só
    // no visual — é o que o teste confere.
    function escolher(a, m, d) {
        var iso = _iso(a, m, d);
        if (!_permitido(iso))
            return false;
        selecionado = iso;
        escolhido(iso);
        close();
        return true;
    }

    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Theme.spacingMd
    width: 300

    background: Rectangle {
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.borderStrong
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingSm

        // Mês e setas
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            ToolButton {
                objectName: "mesAnterior"
                implicitWidth: 34; implicitHeight: 34
                onClicked: cal.mesAnterior()
                contentItem: AppIcon { name: "chevron"; rotation: 90; size: 18; color: Theme.text }
                background: Rectangle { radius: 17; color: parent.hovered ? Theme.surfaceAlt : "transparent" }
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: {
                    var nome = cal.localBr.standaloneMonthName(cal.mes, Locale.LongFormat);
                    return nome.charAt(0).toUpperCase() + nome.slice(1) + " de " + cal.ano;
                }
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
            }
            ToolButton {
                objectName: "proximoMes"
                implicitWidth: 34; implicitHeight: 34
                enabled: cal.podeAvancar
                onClicked: cal.proximoMes()
                contentItem: AppIcon {
                    name: "chevron"; rotation: -90; size: 18
                    color: parent.enabled ? Theme.text : Theme.border
                }
                background: Rectangle { radius: 17; color: parent.hovered && parent.enabled ? Theme.surfaceAlt : "transparent" }
            }
        }

        DayOfWeekRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
            locale: cal.localBr
            delegate: Text {
                required property var model
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                // "dom." -> "D": uma letra basta e cabe sem cortar.
                text: model.shortName.charAt(0).toUpperCase()
                color: Theme.textMuted
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
            }
        }

        MonthGrid {
            id: grade
            objectName: "gradeDias"
            Layout.fillWidth: true
            // O MonthGrid não informa a própria altura: sem isto o layout dava
            // quase zero e as 6 linhas de dias saíam uma por cima da outra.
            // Sempre 6 linhas, mesmo em mês que caberia em 5.
            Layout.preferredHeight: 6 * cal.alturaDia
            month: cal.mes
            year: cal.ano
            locale: cal.localBr

            delegate: Rectangle {
                id: diaCel
                required property var model
                readonly property bool doMes: model.month === grade.month
                readonly property string iso: cal._iso(model.year, model.month, model.day)
                readonly property bool permitido: cal._permitido(iso)
                readonly property bool marcado: iso === cal.selecionado

                implicitWidth: 36
                implicitHeight: cal.alturaDia - 2
                radius: height / 2
                // Transparente, NUNCA invisível: a grade não reserva lugar para item
                // invisível, e os dias do mês escorregavam para a coluna errada
                // (o 14/09/2026, uma segunda, aparecia no sábado).
                opacity: doMes ? 1 : 0
                color: marcado ? Theme.primary
                       : (areaDia.containsMouse && permitido ? Theme.surfaceAlt : "transparent")
                // Hoje ganha um aro, para ninguém se perder no mês.
                border.width: model.today && !marcado ? 1 : 0
                border.color: Theme.primary

                Text {
                    anchors.centerIn: parent
                    text: diaCel.model.day
                    color: diaCel.marcado ? "#15100A"
                           : (diaCel.permitido ? Theme.text : Theme.border)
                    font.pixelSize: Theme.fontSm
                    font.weight: diaCel.marcado || diaCel.model.today ? Font.Bold : Font.Normal
                }

                MouseArea {
                    id: areaDia
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: diaCel.doMes && diaCel.permitido
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: cal.escolher(diaCel.model.year, diaCel.model.month, diaCel.model.day)
                }
            }
        }
    }
}
