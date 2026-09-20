import QtQuick
import QtTest
import Distribuidora

// Aviso de custo fora do normal, na Compra e na Entrada de estoque.
//
// No banco da loja, os custos errados eram sempre o mesmo engano: custo da
// lata lançado na caixinha (fica 12× barato) ou o da caixinha lançado na lata
// (fica acima do preço de venda). O sistema aceitava calado e o lucro do
// produto ficava errado para sempre.
//
// O aviso não impede — pode ser promoção ou produto vendido no prejuízo de
// propósito. Ele exige um segundo clique.
TestCase {
    id: caso
    name: "CustoAviso"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cCompras; ComprasScreen {} }
    Component { id: cEstoque; EstoqueScreen {} }

    property int produtoId: 0
    property int unidadeId: 0
    property int caixaId: 0

    function initTestCase() {
        var p = App.novoProduto();
        p.nome = "Zzz Aviso Custo Lata";
        p.categoriaId = App.categorias()[0].id;
        p.embalagens = [
            { id: 0, nome: "Unidade", fator: 1, codigoBarras: "", preco: 450, custo: -1 },
            { id: 0, nome: "Caixinha", fator: 12, codigoBarras: "", preco: 4800, custo: -1 }
        ];
        verify(App.salvarProduto(p), App.ultimoErro());
        produtoId = App.buscarProdutosPorNome(p.nome)[0].produtoId;
        var embs = App.embalagensDe(produtoId);
        for (var i = 0; i < embs.length; i++) {
            if (embs[i].fator === 1) unidadeId = embs[i].id;
            if (embs[i].fator === 12) caixaId = embs[i].id;
        }
        verify(unidadeId > 0 && caixaId > 0);
    }

    function abrirCompraComCaixinha(custo) {
        var t = createTemporaryObject(cCompras, palco, { width: 1160, height: 740 });
        verify(t !== null, cCompras.errorString());
        var dlg = findChild(t, "novaCompraDialog");
        dlg.abrir();
        dlg.adicionarProduto({ produtoId: produtoId, nome: "Zzz Aviso Custo Lata",
                               embalagemId: caixaId, fator: 12 });
        wait(0);
        var campo = findChild(dlg.contentItem, "custoItemCompra");
        verify(campo !== null);
        campo.text = custo;
        wait(0);
        return dlg;
    }

    // Custo da lata na caixinha: avisa, o 1º clique não grava, o 2º grava.
    function test_compra_custo_baixo_pede_segundo_clique() {
        var dlg = abrirCompraComCaixinha("3,00");
        var aviso = findChild(dlg.contentItem, "avisoCusto");
        verify(aviso.visible, "o aviso não apareceu para custo de 1 lata na caixinha");
        verify(aviso.text.indexOf("pode estar errado") >= 0, aviso.text);

        var antes = App.estoqueDisponivel(produtoId);
        var botao = findChild(dlg, "registrarCompra");
        botao.clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes, "gravou sem conferir o custo");
        verify(dlg.opened);
        verify(findChild(dlg, "erroCompra").text.indexOf("Zzz Aviso Custo Lata") >= 0);
        compare(botao.text, "Registrar mesmo assim");

        botao.clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes + 12);
        verify(!dlg.opened);
    }

    // Corrigir o custo tira o aviso e zera a conferência.
    function test_compra_corrigir_custo_tira_o_aviso() {
        var dlg = abrirCompraComCaixinha("3,00");
        var botao = findChild(dlg, "registrarCompra");
        botao.clicked();
        wait(0);
        compare(botao.text, "Registrar mesmo assim");

        findChild(dlg.contentItem, "custoItemCompra").text = "36,00";
        wait(0);
        verify(!findChild(dlg.contentItem, "avisoCusto").visible);
        compare(botao.text, "Registrar compra");
    }

    // Custo normal passa direto, com um clique.
    function test_compra_custo_normal_grava_de_primeira() {
        var dlg = abrirCompraComCaixinha("36,00");
        verify(!findChild(dlg.contentItem, "avisoCusto").visible);
        var antes = App.estoqueDisponivel(produtoId);
        findChild(dlg, "registrarCompra").clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes + 12);
    }

    // Entrada de estoque: custo da caixinha lançado na unidade.
    function test_entrada_custo_alto_pede_segundo_clique() {
        var t = createTemporaryObject(cEstoque, palco, { width: 1160, height: 740 });
        verify(t !== null, cEstoque.errorString());
        t.abrirMov(produtoId);
        var dlg = findChild(t, "movDialog");
        var combo = findChild(dlg, "embComboEntrada");
        combo.currentIndex = combo.indexOfValue(unidadeId);
        findChild(dlg, "custoEntrada").text = "36,00";
        wait(0);

        var aviso = findChild(dlg, "avisoCustoEntrada");
        verify(aviso.visible, "o aviso não apareceu para custo da caixinha na lata");
        verify(aviso.text.indexOf("pode estar errado") >= 0, aviso.text);

        var antes = App.estoqueDisponivel(produtoId);
        var botao = findChild(dlg, "confirmarMovimento");
        botao.clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes, "gravou sem conferir o custo");
        compare(botao.text, "Confirmar mesmo assim");

        botao.clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes + 1);
    }

    // Entrada sem custo ("vazio mantém o custo") nunca avisa.
    function test_entrada_sem_custo_nao_avisa() {
        var t = createTemporaryObject(cEstoque, palco, { width: 1160, height: 740 });
        t.abrirMov(produtoId);
        var dlg = findChild(t, "movDialog");
        wait(0);
        verify(!findChild(dlg, "avisoCustoEntrada").visible);
        var antes = App.estoqueDisponivel(produtoId);
        findChild(dlg, "confirmarMovimento").clicked();
        wait(0);
        compare(App.estoqueDisponivel(produtoId), antes + 1);
    }
}
