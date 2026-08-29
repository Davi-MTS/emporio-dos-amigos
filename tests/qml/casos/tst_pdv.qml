import QtQuick
import QtTest
import Distribuidora

// PDV — os defeitos que apareceram em uso vieram todos daqui:
//  · o carrinho perdia os insumos do composto (ListModel não guarda array de
//    objetos), e a venda travava em "Escolha os insumos do produto";
//  · o composto exigia estoque, apesar de ser feito na hora.
// Testar só o C++ não pegava nenhum dos dois: a lógica mora no .qml.
TestCase {
    id: caso
    name: "Pdv"
    width: 1200
    height: 760
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cPdv; PdvScreen {} }

    property int idSimples: 0
    property int idComposto: 0
    property int categoriaInsumo: 0

    function initTestCase() {
        var cats = App.categorias();
        verify(cats.length > 0, "o seed precisa trazer categorias");
        categoriaInsumo = cats[0].id;

        idSimples  = criarProduto("Zzz Refri Teste", categoriaInsumo, 500, false);
        idComposto = criarProduto("Zzz Copao Teste", categoriaInsumo, 1500, true);
        verify(idSimples > 0 && idComposto > 0);
    }

    function criarProduto(nome, catId, preco, composto) {
        var p = App.novoProduto();
        p.nome = nome;
        p.categoriaId = catId;
        p.composto = composto;
        // Monta a lista inteira: mexer em p.embalagens[0].preco não volta para
        // o mapa (a lista vem do C++ e cada item é uma cópia).
        p.embalagens = [{ id: 0, nome: "Unidade", fator: 1,
                          codigoBarras: "", preco: preco, custo: -1 }];
        if (composto)
            p.composicao = [{ categoriaId: catId, unidade: "unidade", quantidade: 1 }];
        verify(App.salvarProduto(p), "não salvou " + nome + ": " + App.ultimoErro());
        var achados = App.buscarProdutosPorNome(nome, true);
        return achados.length > 0 ? achados[0].produtoId : 0;
    }

    function item(id) {
        var l = App.buscarProdutosPorNome(id === idSimples ? "Zzz Refri" : "Zzz Copao", true);
        verify(l.length > 0);
        return l[0];
    }

    function novoPdv() {
        var t = createTemporaryObject(cPdv, palco, { width: 1160, height: 700 });
        verify(t !== null, cPdv.errorString());
        wait(0);
        return t;
    }

    // Produto normal: soma no total e avisa quando falta estoque (sem bloquear).
    function test_produto_simples_soma_e_avisa_estoque() {
        var pdv = novoPdv();
        pdv.adicionar(item(idSimples));
        compare(pdv.totalVenda, 500, "o total tem que ser o preço da unidade");
        // Produto recém-criado tem estoque zero: o aviso deve aparecer.
        verify(pdv.avisoEstoque.length > 0, "deveria avisar que falta estoque");
    }

    // Adicionar duas vezes agrupa na mesma linha em vez de duplicar.
    function test_produto_simples_agrupa() {
        var pdv = novoPdv();
        pdv.adicionar(item(idSimples));
        pdv.adicionar(item(idSimples));
        compare(pdv.totalVenda, 1000);
    }

    // O caso que travava a venda: composto entra pelo preço escolhido e NÃO
    // exige estoque, nem dele nem dos insumos.
    function test_composto_nao_exige_estoque() {
        var pdv = novoPdv();
        var escolhas = [{ produtoId: idSimples, quantidade: 1, nome: "Zzz Refri Teste" }];
        pdv._adicionarComposto(item(idComposto), escolhas, 1500);
        compare(pdv.totalVenda, 1500);
        compare(pdv.avisoEstoque, "", "composto não pode pedir estoque");
    }

    // A receita do composto chega inteira ao carrinho (era aqui que os insumos
    // sumiam e a finalização acusava "Escolha os insumos do produto").
    function test_composto_preserva_insumos() {
        var pdv = novoPdv();
        var escolhas = [
            { produtoId: idSimples, quantidade: 2, nome: "Zzz Refri Teste" },
            { produtoId: idSimples, quantidade: 1, nome: "Zzz Refri Teste" }
        ];
        pdv._adicionarComposto(item(idComposto), escolhas, 1800);

        var linha = pdv.linhaCarrinho(0);
        verify(linha !== null, "o carrinho ficou vazio");
        var insumos = JSON.parse(linha.insumosJson);
        compare(insumos.length, 2, "os insumos escolhidos precisam sobreviver ao ListModel");
        compare(insumos[0].produtoId, idSimples);
        compare(insumos[0].quantidade, 2);
        verify(linha.insumosLabel.length > 0, "a linha precisa mostrar a receita");
    }

    // Limpar tem que zerar tudo — total, pagamentos e aviso.
    function test_limpar_zera() {
        var pdv = novoPdv();
        pdv.adicionar(item(idSimples));
        pdv.limparVenda();
        compare(pdv.totalVenda, 0);
        compare(pdv.pagoVenda, 0);
        compare(pdv.avisoEstoque, "");
    }
}
