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

    // A embalagem fica AO LADO do nome, em coluna própria — não empilhada
    // embaixo dele. E na MESMA posição em toda linha, tenha o produto uma
    // embalagem (texto) ou várias (seletor): senão as linhas dançam.
    function test_embalagem_ao_lado_do_nome() {
        // O carrinho só aparece com o caixa aberto: fechado, a tela mostra o
        // painel "Caixa fechado" e a lista inteira fica invisível.
        App.abrirCaixa("50,00");
        var p = App.novoProduto();
        p.nome = "Zzz Palheiro Teste";
        p.categoriaId = categoriaInsumo;
        p.embalagens = [{ id: 0, nome: "Unidade", fator: 1, codigoBarras: "", preco: 250, custo: -1 },
                        { id: 0, nome: "Caixinha", fator: 10, codigoBarras: "", preco: 2300, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());

        var pdv = novoPdv();
        pdv.adicionar(App.buscarProdutosPorNome("Zzz Palheiro Teste", true)[0]);
        pdv.adicionar(item(idSimples));   // esse tem uma embalagem só
        wait(0);
        waitForRendering(pdv);   // sem um quadro desenhado o ListView nem cria as linhas

        var lista = findChild(pdv, "listaCarrinho");
        verify(lista !== null, "não achei o carrinho");
        compare(lista.count, 2);
        verify(pdv.mostrarEmbalagem, "a 1160 px a coluna da embalagem tem que caber");

        var xs = [];
        for (var i = 0; i < 2; i++) {
            var celula = findChild(lista.itemAtIndex(i), "embalagemCarrinho");
            verify(celula !== null, "linha " + i + " sem a coluna de embalagem");
            verify(celula.visible, "a coluna de embalagem sumiu da linha " + i);
            xs.push(Math.round(celula.mapToItem(lista, 0, 0).x));
        }
        compare(xs[1], xs[0], "a coluna tem que começar no mesmo x nas duas linhas");

        // Produto com escolha mostra o seletor; o de uma embalagem, não.
        var combo0 = findChild(lista.itemAtIndex(0), "comboEmbalagemCarrinho");
        verify(combo0 !== null && combo0.visible, "produto com 2 embalagens precisa do seletor");
        var combo1 = findChild(lista.itemAtIndex(1), "comboEmbalagemCarrinho");
        verify(combo1 === null || !combo1.visible, "com uma embalagem só não há o que escolher");

        // AO LADO: o seletor começa depois de onde o nome termina.
        var nomeDireita = xs[0];
        verify(combo0.mapToItem(lista, 0, 0).x >= nomeDireita - 1,
               "o seletor não está na coluna da embalagem");

        App.fecharCaixa("50,00");
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
