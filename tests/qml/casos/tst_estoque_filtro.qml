import QtQuick
import QtTest
import Distribuidora

// Filtro do Estoque por situação (Zerados / Baixo / OK).
//
// O que não pode acontecer: o filtro discordar do selo da coluna Status (a
// pessoa filtra "Baixo" e aparece um produto marcado "OK"), a contagem do botão
// não bater com a lista, ou o filtro sumir sozinho no meio do trabalho.
TestCase {
    id: caso
    name: "EstoqueFiltro"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cEstoque; EstoqueScreen {} }

    // Prefixo único: o banco do arnês é compartilhado com os outros casos, então
    // a busca por nome isola só os produtos deste teste.
    readonly property string prefixo: "Zzz Filtro Estoque"
    property int idZerado: 0
    property int idBaixo: 0
    property int idOk: 0

    function criar(sufixo, minimo, entrada) {
        var p = App.novoProduto();
        p.nome = prefixo + " " + sufixo;
        p.categoriaId = App.categorias()[0].id;
        p.estoqueMinimo = minimo;
        verify(App.salvarProduto(p), App.ultimoErro());
        var id = App.buscarProdutosPorNome(p.nome)[0].produtoId;
        if (entrada > 0) {
            var emb = App.embalagensDe(id)[0].id;
            verify(App.registrarEntrada(id, emb, entrada, "1,00", "", "", ""), App.ultimoErro());
        }
        return id;
    }

    function initTestCase() {
        idZerado = criar("Zerado", 5, 0);    // 0 em estoque
        idBaixo  = criar("Baixo", 10, 3);    // 3, mínimo 10
        idOk     = criar("Ok", 2, 50);       // 50, mínimo 2
    }

    function abrir() {
        var t = createTemporaryObject(cEstoque, palco, { width: 1160, height: 740 });
        verify(t !== null, cEstoque.errorString());
        App.recarregarEstoque(prefixo);
        wait(0);
        return t;
    }

    function cleanup() {
        App.estoque.filtroStatus = "";
        App.recarregarEstoque("");
    }

    function test_contagem_por_situacao() {
        abrir();
        var c = App.estoque.contagem;
        compare(c.todos, 3);
        compare(c.zerado, 1);
        compare(c.baixo, 1);
        compare(c.ok, 1);
    }

    // Cada filtro mostra só quem tem aquele selo — a mesma regra, sempre.
    function test_filtro_bate_com_o_selo() {
        abrir();
        var casos = [["zerado", "Zerado"], ["baixo", "Baixo"], ["ok", "Ok"]];
        for (var i = 0; i < casos.length; i++) {
            App.estoque.filtroStatus = casos[i][0];
            wait(0);
            compare(App.estoque.rowCount(), 1, "filtro " + casos[i][0]);
            var idx = App.estoque.index(0, 0);
            compare(App.estoque.data(idx, 257 + 7), casos[i][0],   // StatusRole
                    "o selo da linha não é o do filtro " + casos[i][0]);
        }
        App.estoque.filtroStatus = "";
        compare(App.estoque.rowCount(), 3, "Todos tem que voltar a mostrar tudo");
    }

    // O botão da tela liga o filtro de verdade (não é só enfeite).
    function test_botao_da_tela_filtra() {
        var tela = abrir();
        var seg = findChild(tela, "filtroStatusEstoque");
        verify(seg !== null, "filtro não está na tela de Estoque");
        compare(seg.options.length, 4);
        verify(seg.options[1].indexOf("Zerados") === 0);
        verify(seg.options[1].indexOf("1") > 0, "o botão tem que mostrar quantos há: " + seg.options[1]);

        seg.currentIndex = 1;   // Zerados
        wait(0);
        compare(App.estoque.filtroStatus, "zerado");
        compare(App.estoque.rowCount(), 1);
    }

    // Dar entrada num produto zerado recarrega a lista: com o filtro ligado ele
    // tem que SAIR de "Zerados" — é o retorno de que o trabalho andou.
    function test_filtro_sobrevive_a_entrada() {
        abrir();
        App.estoque.filtroStatus = "zerado";
        compare(App.estoque.rowCount(), 1);

        var emb = App.embalagensDe(idZerado)[0].id;
        verify(App.registrarEntrada(idZerado, emb, 20, "1,00", "", "", ""), App.ultimoErro());
        App.recarregarEstoque(prefixo);
        wait(0);

        compare(App.estoque.filtroStatus, "zerado", "o filtro não pode desligar sozinho");
        compare(App.estoque.rowCount(), 0, "o produto abastecido tinha que sair de Zerados");
        compare(App.estoque.contagem.zerado, 0);
    }

    // Sair da tela desliga o filtro: o model é um só para o app inteiro.
    function test_sair_da_tela_desliga_o_filtro() {
        var t = abrir();
        App.estoque.filtroStatus = "baixo";
        t.destroy();
        wait(0);
        compare(App.estoque.filtroStatus, "");
    }

    // Valor desconhecido vira "todos", nunca uma lista vazia sem explicação.
    function test_filtro_invalido_mostra_tudo() {
        abrir();
        App.estoque.filtroStatus = "qualquer coisa";
        compare(App.estoque.filtroStatus, "");
        compare(App.estoque.rowCount(), 3);
    }
}
