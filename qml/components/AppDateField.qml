import QtQuick
import Distribuidora

// Campo de data que põe as barras sozinho: digita-se 09062026 e aparece
// 09/06/2026.
//
// Antes cada tela tinha um campo de texto solto pedindo "AAAA-MM-DD", e o que
// a pessoa escrevesse ia direto para o banco. Uma conta gravada como
// "20260829" nunca aparecia como vencida — a comparação de data é textual, e
// "20260829" é maior que "2026-08-31". Por isso o campo é um só e devolve
// sempre `iso`, no formato que o banco entende.
//
// Uso:
//   AppDateField { id: venc }
//   ... venc.iso        -> "2026-08-29" (vazio se incompleto ou inexistente)
//   ... venc.definir("2026-08-29")   para carregar um valor existente
AppTextField {
    id: control

    // Data em ISO (yyyy-MM-dd). Vazio quando o campo está vazio, incompleto ou
    // com uma data que não existe (31/02, por exemplo).
    readonly property string iso: _iso(text)
    // Escreveu alguma coisa mas ainda não é uma data válida.
    readonly property bool incompleto: text.length > 0 && iso.length === 0

    placeholderText: qsTr("dd/mm/aaaa")
    maximumLength: 10
    inputMethodHints: Qt.ImhDigitsOnly
    horizontalAlignment: Text.AlignHCenter
    color: incompleto ? Theme.danger : Theme.text

    function definir(isoTexto) {
        if (!isoTexto || isoTexto.length === 0) {
            text = "";
            return;
        }
        var m = ("" + isoTexto).match(/^(\d{4})-(\d{2})-(\d{2})/);
        text = m ? (m[3] + "/" + m[2] + "/" + m[1]) : ("" + isoTexto);
    }

    function limpar() { text = ""; }

    // Reescreve o que foi digitado no formato dd/mm/aaaa, ignorando tudo que
    // não for dígito (inclusive as barras que a pessoa digitar por conta).
    function _mascarar(bruto) {
        var d = ("" + bruto).replace(/\D/g, "").substring(0, 8);
        var r = d.substring(0, 2);
        if (d.length > 2)
            r += "/" + d.substring(2, 4);
        if (d.length > 4)
            r += "/" + d.substring(4, 8);
        return r;
    }

    // Só devolve ISO quando a data EXISTE: 31/02/2026 não passa.
    function _iso(t) {
        var m = ("" + t).match(/^(\d{2})\/(\d{2})\/(\d{4})$/);
        if (!m)
            return "";
        var dia = parseInt(m[1]);
        var mes = parseInt(m[2]);
        var ano = parseInt(m[3]);
        var d = new Date(ano, mes - 1, dia);
        if (isNaN(d.getTime()) || d.getDate() !== dia || d.getMonth() !== mes - 1
            || d.getFullYear() !== ano)
            return "";
        return Qt.formatDate(d, "yyyy-MM-dd");
    }

    onTextChanged: {
        var f = _mascarar(text);
        if (f === text)
            return;
        // Mantém o cursor onde a pessoa estava: inserir a barra empurra um a mais.
        var novoCursor = cursorPosition + (f.length - text.length);
        text = f;
        cursorPosition = Math.max(0, Math.min(f.length, novoCursor));
    }
}
