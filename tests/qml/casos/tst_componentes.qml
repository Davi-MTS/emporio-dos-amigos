import QtQuick
import QtQuick.Controls
import QtTest
import Distribuidora

// Peças reutilizadas por todas as telas. Quando uma delas quebra, quebra em
// tudo ao mesmo tempo — e foi onde apareceram os defeitos de uso: diálogo mais
// alto que a janela (botão de confirmar fora do alcance) e campo cortando o que
// o operador digitou.
TestCase {
    id: caso
    name: "Componentes"
    width: 900
    height: 640
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }

    Component {
        id: cDialogAlto
        AppDialog {
            title: "Diálogo alto"
            contentItem: Item { implicitHeight: 4000; implicitWidth: 400 }
        }
    }
    Component { id: cToggle; ToggleButton { text: "Ligar" } }
    Component { id: cSeg;    SegmentedControl { options: ["Entrada", "Inventário"]; width: 260 } }
    Component { id: cCampo;  AppTextField {} }

    // Diálogo nunca pode passar da janela: se passar, os botões de confirmar
    // ficam fora da tela e a operação empaca.
    function test_dialogo_nao_passa_da_janela() {
        var d = createTemporaryObject(cDialogAlto, palco);
        verify(d !== null, cDialogAlto.errorString());
        d.open();
        wait(0);
        verify(d.height <= palco.height,
               "diálogo com " + d.height + "px numa janela de " + palco.height + "px");
        verify(d.height > 0);
        d.close();
    }

    // Botão de alternância (o "botão bonito" que substituiu a caixinha).
    function test_toggle_alterna() {
        var t = createTemporaryObject(cToggle, palco);
        verify(t !== null, cToggle.errorString());
        compare(t.checked, false);
        t.toggle();
        compare(t.checked, true, "clicar tem que ligar");
        t.toggle();
        compare(t.checked, false);
    }

    // Alternador de Entrada/Inventário: o destaque acompanha o índice.
    function test_segmented_troca_indice() {
        var s = createTemporaryObject(cSeg, palco, { width: 260, height: 40 });
        verify(s !== null, cSeg.errorString());
        compare(s.currentIndex, 0);
        s.currentIndex = 1;
        wait(0);
        compare(s.currentIndex, 1);
        compare(s.options.length, 2);
    }

    // Campos pequenos cortavam o que foi digitado (data de nascimento, valor).
    // O texto tem que caber dentro da área útil do campo.
    function test_campo_nao_corta_texto_data() {
        naoCorta("31/12/1990", 200);
        naoCorta("R$ 1.234,56", 200);
        naoCorta("(62) 99999-9999", 220);
    }

    function naoCorta(texto, largura) {
        var c = createTemporaryObject(cCampo, palco, { width: largura });
        verify(c !== null, cCampo.errorString());
        c.text = texto;
        wait(0);
        var util = c.width - c.leftPadding - c.rightPadding;
        verify(c.contentWidth <= util,
               "'" + texto + "' precisa de " + Math.ceil(c.contentWidth)
               + "px e o campo só tem " + Math.floor(util) + "px úteis");
    }
}
