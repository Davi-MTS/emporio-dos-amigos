import QtQuick
import QtTest
import Distribuidora

// Relatório de um dia específico, escolhido num calendário.
//
// O que não pode acontecer: o calendário abrir no mês errado, deixar escolher
// um dia no futuro (não tem venda, e a tela zerada assusta), ou a data escorregar
// um dia por causa do fuso — o `date` do MonthGrid pode vir em UTC, e às 21h de
// Goiânia isso já é o dia anterior.
TestCase {
    id: caso
    name: "RelatoriosDia"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cRelatorios; RelatoriosScreen {} }

    function abrir() {
        var t = createTemporaryObject(cRelatorios, palco, { width: 1160, height: 740 });
        verify(t !== null, cRelatorios.errorString());
        wait(0);
        return t;
    }

    function iso(d) { return Qt.formatDate(d, "yyyy-MM-dd"); }

    function test_outro_dia_mostra_botao_e_abre_em_ontem() {
        var t = abrir();
        var periodo = findChild(t, "periodoRelatorio");
        var botao = findChild(t, "diaRelatorio");
        verify(periodo !== null && botao !== null);
        compare(periodo.options.length, 4);
        compare(botao.visible, false, "o botão do dia só aparece em Outro dia");

        periodo.currentIndex = 3;
        wait(0);
        compare(botao.visible, true);
        var ontem = new Date(); ontem.setDate(ontem.getDate() - 1);
        compare(t.dia, iso(ontem), "Outro dia abre em ontem");
        verify(botao.text.indexOf(Qt.formatDate(ontem, "dd/MM/yyyy")) >= 0,
               "o botão tem que mostrar o dia escolhido: " + botao.text);
    }

    // Clicar no botão abre o calendário JÁ no mês do dia escolhido.
    function test_calendario_abre_no_mes_do_dia() {
        var t = abrir();
        findChild(t, "periodoRelatorio").currentIndex = 3;
        wait(0);
        t.dia = "2025-02-10";

        var botao = findChild(t, "diaRelatorio");
        botao.clicked();
        wait(0);
        var cal = findChild(t, "calendarioRelatorio");
        verify(cal !== null, "calendário não encontrado");
        compare(cal.opened, true, "clicar no botão tem que abrir o calendário");
        // A grade de dias colapsava para quase zero de altura e as 6 linhas
        // saíam sobrepostas. Cada linha precisa de pelo menos ~28 px.
        wait(50);
        var grade = findChild(cal.contentItem, "gradeDias");
        verify(grade !== null, "grade de dias não encontrada");
        verify(grade.height >= 6 * 28, "a grade de dias colapsou: altura " + grade.height);
        compare(cal.ano, 2025);
        compare(cal.mes, 1);   // fevereiro (0 = janeiro, como no JavaScript)
        cal.close();
    }

    // Escolher um dia troca o relatório e fecha o calendário.
    function test_escolher_dia_troca_o_relatorio() {
        var t = abrir();
        findChild(t, "periodoRelatorio").currentIndex = 3;
        wait(0);
        var cal = findChild(t, "calendarioRelatorio");
        cal.abrirEm("2020-01-15");
        wait(0);

        verify(cal.escolher(2020, 0, 1));
        wait(0);
        compare(t.dia, "2020-01-01", "a data montada por inteiros não pode escorregar de dia");
        compare(cal.opened, false, "o calendário fecha depois de escolher");
        compare(t.fat.numVendas, 0, "não houve venda em 2020");
    }

    // Dia no futuro não tem venda: não pode ser escolhido, nem avançar para um
    // mês inteiro à frente.
    function test_futuro_nao_pode_ser_escolhido() {
        var t = abrir();
        findChild(t, "periodoRelatorio").currentIndex = 3;
        wait(0);
        var cal = findChild(t, "calendarioRelatorio");
        var hoje = new Date();
        cal.abrirEm(iso(hoje));
        wait(0);

        var amanha = new Date(hoje.getFullYear(), hoje.getMonth(), hoje.getDate() + 1);
        var antes = t.dia;
        compare(cal.escolher(amanha.getFullYear(), amanha.getMonth(), amanha.getDate()), false);
        compare(t.dia, antes, "dia futuro não pode virar o dia do relatório");

        compare(cal.podeAvancar, false, "não pode avançar para o mês que vem");
        cal.mesAnterior();
        compare(cal.podeAvancar, true, "voltando um mês, avançar volta a ser permitido");
        cal.close();
    }

    // Cada dia tem que cair na coluna do seu dia da semana. Escondendo os dias
    // do mês anterior com `visible: false`, a grade não reservava o lugar deles
    // e o mês inteiro escorregava para a esquerda.
    function test_dia_cai_na_coluna_do_dia_da_semana() {
        var t = abrir();
        findChild(t, "periodoRelatorio").currentIndex = 3;
        wait(0);
        var cal = findChild(t, "calendarioRelatorio");
        cal.abrirEm("2026-09-10");
        wait(50);
        var grade = findChild(cal.contentItem, "gradeDias");
        verify(grade !== null);

        var largura = grade.width / 7;
        var conferidos = 0;
        var celulas = grade.contentItem.children;
        for (var i = 0; i < celulas.length; i++) {
            var c = celulas[i];
            if (c.iso === "2026-09-01" || c.iso === "2026-09-14" || c.iso === "2026-09-30") {
                var p = c.iso.split("-");
                var esperado = new Date(parseInt(p[0]), parseInt(p[1]) - 1, parseInt(p[2])).getDay(); // 0 = domingo
                compare(Math.round(c.x / largura), esperado,
                        c.iso + " está na coluna errada");
                conferidos++;
            }
        }
        compare(conferidos, 3, "não achei os dias na grade");
        cal.close();
    }

    function test_voltar_para_hoje_desliga_o_dia() {
        var t = abrir();
        var periodo = findChild(t, "periodoRelatorio");
        periodo.currentIndex = 3;
        wait(0);
        periodo.currentIndex = 0;
        wait(0);
        compare(t.dia, "", "Hoje não pode continuar preso no dia escolhido");
        compare(t.dias, 0);
    }
}
