import QtQuick
import QtTest
import Distribuidora

// Estoque mínimo: o banco guarda em unidade base, mas ninguém pensa "avise
// quando faltar 2000 ml" — pensa "avise quando sobrar menos de 2 garrafas".
// A tela digita por embalagem e converte. Se a conversão inverter ou sumir, o
// alerta de estoque baixo passa a disparar na hora errada (ou nunca), e isso
// só se descobre quando a mercadoria acaba na prateleira.
TestCase {
    id: caso
    name: "Produtos"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cProdutos; ProdutosScreen {} }

    property int idGarrafa: 0

    function initTestCase() {
        var cat = App.categorias()[0].id;

        // Whisky contado em ml, com garrafa de 1 litro (fator 1000).
        var p = App.novoProduto();
        p.nome = "Zzz Whisky Minimo";
        p.categoriaId = cat;
        p.unidadeBase = "ml";
        p.estoqueMinimo = 2000;          // dois litros, em unidade base
        p.embalagens = [{ id: 0, nome: "Garrafa 1 L", fator: 1000,
                          codigoBarras: "", preco: 12000, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());

        var achados = App.buscarProdutosPorNome("Zzz Whisky Minimo", true);
        verify(achados.length > 0, "produto de teste não foi criado");
        idGarrafa = achados[0].produtoId;
    }

    function abrirTela() {
        var t = createTemporaryObject(cProdutos, palco, { width: 1160, height: 740 });
        verify(t !== null, cProdutos.errorString());
        wait(0);
        return t;
    }

    // 2000 ml tem que voltar como "2 × Garrafa 1 L", e não como "2000".
    function test_minimo_volta_na_embalagem_maior() {
        var tela = abrirTela();
        tela.abrirProduto(idGarrafa);
        wait(0);

        compare(tela.unidadeBase, "ml");
        compare(tela.fatorMinimo(), 1000, "deveria escolher a garrafa, não a unidade base");

        // O texto de apoio precisa dizer o valor REAL em unidade base, senão o
        // operador não liga "2 garrafas" a "2000 ml".
        var texto = tela.explicarMinimo();
        verify(texto.indexOf("2000") >= 0, "texto não mostra o valor em unidade base: " + texto);
        verify(texto.indexOf("ml") >= 0, texto);
    }

    // Produto normal (unidade), mínimo continua sendo o número digitado.
    function test_produto_em_unidade_nao_muda_nada() {
        var cat = App.categorias()[0].id;
        var p = App.novoProduto();
        p.nome = "Zzz Lata Minimo";
        p.categoriaId = cat;
        p.unidadeBase = "unidade";
        p.estoqueMinimo = 12;
        p.embalagens = [{ id: 0, nome: "Unidade", fator: 1,
                          codigoBarras: "", preco: 500, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());
        var id = App.buscarProdutosPorNome("Zzz Lata Minimo", true)[0].produtoId;

        var tela = abrirTela();
        tela.abrirProduto(id);
        wait(0);
        compare(tela.fatorMinimo(), 1);
        verify(tela.explicarMinimo().indexOf("12") >= 0, tela.explicarMinimo());
    }

    // Mínimo zero significa "não me avise", e a tela tem que dizer isso.
    function test_minimo_zero_avisa_que_nao_avisa() {
        var cat = App.categorias()[0].id;
        var p = App.novoProduto();
        p.nome = "Zzz Sem Minimo";
        p.categoriaId = cat;
        p.estoqueMinimo = 0;
        verify(App.salvarProduto(p), App.ultimoErro());
        var id = App.buscarProdutosPorNome("Zzz Sem Minimo", true)[0].produtoId;

        var tela = abrirTela();
        tela.abrirProduto(id);
        wait(0);
        verify(tela.explicarMinimo().toLowerCase().indexOf("sem aviso") >= 0,
               tela.explicarMinimo());
    }

    // A unidade base virou propriedade da tela (o campo de texto saiu): trocar
    // pelos botões tem que continuar mudando o que é salvo.
    function test_unidade_base_e_propriedade_da_tela() {
        var tela = abrirTela();
        tela.abrirNovo();
        wait(0);
        compare(tela.unidadeBase, "unidade");
        tela.unidadeBase = "ml";
        compare(tela.explicarUnidade(tela.unidadeBase).indexOf("ML") >= 0, true,
                tela.explicarUnidade(tela.unidadeBase));
    }
}
