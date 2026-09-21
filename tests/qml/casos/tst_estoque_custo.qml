import QtQuick
import QtTest
import Distribuidora

// Aba "Custo" do Estoque: corrigir o custo de um produto que entrou errado.
//
// O caso da loja: PALHEIRO PIRACANJUBA entrou com o custo da caixa (R$ 17,50)
// lançado na unidade e vende a R$ 2,00. Cada venda travou esse custo, e o
// relatório mostra um prejuízo que nunca existiu. Não havia volta — nem
// ajustar custo, nem cancelar compra.
TestCase {
    id: caso
    name: "EstoqueCusto"
    width: 1200
    height: 820
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cEstoque; EstoqueScreen {} }

    property int seq: 0

    // Um produto novo por caso: o custo médio de um não pode vazar para o outro.
    function palheiro(custoEntrada) {
        var p = App.novoProduto();
        p.nome = "Zzz Custo Aba " + (++seq);
        p.categoriaId = App.categorias()[0].id;
        p.embalagens = [{ id: 0, nome: "Unidade", fator: 1, codigoBarras: "", preco: 200, custo: -1 },
                        { id: 0, nome: "Caixinha", fator: 10, codigoBarras: "", preco: 1800, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());
        var r = { id: App.buscarProdutosPorNome(p.nome)[0].produtoId, unidade: 0, caixinha: 0 };
        var embs = App.embalagensDe(r.id);
        for (var i = 0; i < embs.length; i++) {
            if (embs[i].fator === 1) r.unidade = embs[i].id;
            if (embs[i].fator === 10) r.caixinha = embs[i].id;
        }
        verify(App.registrarEntrada(r.id, r.unidade, 20, custoEntrada, "", "", ""), App.ultimoErro());
        return r;
    }

    function vender(prod, vezes) {
        App.abrirCaixa("50,00");
        for (var i = 0; i < vezes; i++) {
            var r = App.finalizarVenda({
                desconto: 0, clienteId: 0,
                itens: [{ produtoId: prod.id, embalagemId: prod.unidade, fator: 1,
                          qtd: 1, precoUnit: 200, desconto: 0 }],
                pagamentos: [{ forma: "pix", valor: 200 }] });
            verify(r.ok, r.erro);
        }
    }

    function cleanup() {
        if (App.caixaAberto)
            App.fecharCaixa("50,00");
        App.recarregarEstoque("");
    }

    // Abre o diálogo do produto já na aba Custo.
    function abrirAba(prod) {
        var t = createTemporaryObject(cEstoque, palco, { width: 1160, height: 780 });
        verify(t !== null, cEstoque.errorString());
        t.abrirMov(prod.id);
        wait(0);
        var dlg = findChild(t, "movDialog");
        verify(dlg !== null);
        verify(findChild(dlg, "abaCusto") !== null, "a aba Custo não existe");

        // O seletor de abas: é o SegmentedControl do diálogo que tem "Custo".
        var seletor = null;
        var fila = [dlg.contentItem];
        while (fila.length > 0 && seletor === null) {
            var it = fila.shift();
            for (var i = 0; i < it.children.length; i++) {
                var f = it.children[i];
                if (f.options !== undefined && f.options.indexOf !== undefined
                        && f.options.indexOf("Custo") >= 0)
                    seletor = f;
                fila.push(f);
            }
        }
        verify(seletor !== null, "o seletor de abas não tem 'Custo'");
        compare(seletor.options.indexOf("Custo"), seletor.options.length - 1,
                "Custo fica por último, como as outras abas restritas");
        seletor.currentIndex = seletor.options.indexOf("Custo");
        wait(0);
        return dlg;
    }

    function escolher(dlg, embId, custo) {
        var combo = findChild(dlg, "custoEmbCombo");
        combo.currentIndex = combo.indexOfValue(embId);
        findChild(dlg, "custoNovoField").text = custo;
        wait(0);
    }

    // O caminho feliz: digita o custo certo da CAIXINHA, vê antes -> depois,
    // e as vendas feitas com o engano são corrigidas junto.
    function test_corrige_custo_e_as_vendas() {
        var prod = palheiro("17,50");
        vender(prod, 2);
        var dlg = abrirAba(prod);
        escolher(dlg, prod.caixinha, "14,60");

        // O campo diz de qual embalagem é o custo.
        compare(findChild(dlg, "rotuloCustoNovo").label, "Novo custo (Caixinha)");
        // A tabela: cada embalagem, antes e depois (a unidade acompanha a caixinha).
        var linhas = dlg.previaCusto.embalagens;
        compare(linhas.length, 2);
        compare(linhas[0].nome, "Unidade");
        compare(linhas[0].atual, 1750);
        compare(linhas[0].novo, 146);
        compare(linhas[1].nome, "Caixinha");
        compare(linhas[1].atual, 17500);
        compare(linhas[1].novo, 1460);
        verify(linhas[1].escolhida, "a embalagem do campo vem destacada");

        var toggle = findChild(dlg, "corrigirVendasCusto");
        verify(toggle.visible, "com vendas desde a entrada o botão de corrigi-las aparece");
        verify(toggle.checked, "ligado por padrão: é para isso que se corrige");
        verify(toggle.text.indexOf("2 vendas") >= 0, toggle.text);

        var lucroAntes = App.relatorioFaturamento(0).lucro;
        findChild(dlg, "confirmarMovimento").clicked();
        wait(0);

        verify(!dlg.opened, "o diálogo fecha quando grava: " + findChild(dlg, "erroMovimento").text);
        compare(App.itemEstoque(prod.id).custoMedio, 146);
        compare(App.itemEstoque(prod.id).quantidade, 18, "a quantidade não se mexe");
        compare(App.relatorioFaturamento(0).lucro - lucroAntes, 2 * (1750 - 146),
                "as 2 vendas passaram a ter o custo certo");
    }

    // Desligando o botão, o custo muda mas as vendas passadas ficam como estão.
    function test_desligar_nao_mexe_nas_vendas() {
        var prod = palheiro("17,50");
        vender(prod, 1);
        var dlg = abrirAba(prod);
        escolher(dlg, prod.unidade, "1,46");
        findChild(dlg, "corrigirVendasCusto").checked = false;

        var lucroAntes = App.relatorioFaturamento(0).lucro;
        findChild(dlg, "confirmarMovimento").clicked();
        wait(0);
        verify(!dlg.opened);
        compare(App.itemEstoque(prod.id).custoMedio, 146);
        compare(App.relatorioFaturamento(0).lucro, lucroAntes);
    }

    // Custo "certo" que ainda dá margem absurda: sinal de que o problema está no
    // FATOR do cadastro. Avisa e pede o segundo clique, como na Entrada.
    function test_custo_absurdo_pede_segundo_clique() {
        var prod = palheiro("1,50");
        var dlg = abrirAba(prod);
        escolher(dlg, prod.unidade, "9,00");   // acima do preço de R$ 2,00

        verify(findChild(dlg, "avisoCustoAjuste").visible, "tinha que avisar");
        var botao = findChild(dlg, "confirmarMovimento");
        botao.clicked();
        wait(0);
        verify(dlg.opened, "o primeiro clique não grava");
        compare(App.itemEstoque(prod.id).custoMedio, 150);
        compare(findChild(dlg, "erroMovimento").text, "Confira o custo");
        compare(botao.text, "Ajustar mesmo assim");

        botao.clicked();
        wait(0);
        compare(App.itemEstoque(prod.id).custoMedio, 900);
    }

    // Sem vendas desde a entrada, não há o que corrigir: o botão nem aparece.
    function test_sem_vendas_nao_oferece_corrigir() {
        var prod = palheiro("1,50");
        var dlg = abrirAba(prod);
        escolher(dlg, prod.unidade, "1,40");
        verify(!findChild(dlg, "corrigirVendasCusto").visible);
    }

    // Custo inválido: recado curto, nada gravado.
    function test_custo_invalido() {
        var prod = palheiro("1,50");
        var dlg = abrirAba(prod);
        escolher(dlg, prod.unidade, "0");
        findChild(dlg, "confirmarMovimento").clicked();
        wait(0);
        verify(dlg.opened);
        compare(findChild(dlg, "erroMovimento").text, "Custo inválido");
        compare(App.itemEstoque(prod.id).custoMedio, 150);
    }

    // Trocar a embalagem mostra o custo DELA — no rótulo e no campo vazio.
    function test_trocar_embalagem_mostra_o_custo_dela() {
        var prod = palheiro("1,50");
        var dlg = abrirAba(prod);
        var campo = findChild(dlg, "custoNovoField");

        escolher(dlg, prod.unidade, "");
        compare(findChild(dlg, "rotuloCustoNovo").label, "Novo custo (Unidade)");
        compare(campo.placeholderText, "1,50");

        escolher(dlg, prod.caixinha, "");
        compare(findChild(dlg, "rotuloCustoNovo").label, "Novo custo (Caixinha)");
        compare(campo.placeholderText, "15,00", "a caixinha de 10 custa 10x a unidade");
        // A tabela aparece mesmo com o campo vazio — é o "atual" de cada uma.
        compare(dlg.previaCusto.embalagens.length, 2);
    }

    // Corrigir pelo custo da caixinha: a unidade acompanha, porque é o mesmo
    // estoque (a lata da caixinha aberta é a lata vendida avulsa).
    function test_custo_da_caixinha_define_a_unidade() {
        var prod = palheiro("1,50");
        var dlg = abrirAba(prod);
        escolher(dlg, prod.caixinha, "18,00");
        findChild(dlg, "confirmarMovimento").clicked();
        wait(0);
        verify(!dlg.opened, findChild(dlg, "erroMovimento").text);
        compare(App.itemEstoque(prod.id).custoMedio, 180);   // 18,00 / 10
    }
}
