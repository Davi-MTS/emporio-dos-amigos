import QtQuick
import QtTest
import Distribuidora

// Correções da auditoria completa (set/2026) que moram na tela.
TestCase {
    id: caso
    name: "Correcoes"
    width: 1300
    height: 900
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cProdutos; ProdutosScreen {} }
    Component { id: cCompras; ComprasScreen {} }
    Component { id: cClientes; ClientesScreen {} }

    // Todos os itens visíveis com esse objectName (Repeater incluso), na ordem.
    function todos(item, nome, acc) {
        if (!item) return acc;
        if (item.objectName === nome && item.visible) acc.push(item);
        var filhos = item.children || [];
        for (var i = 0; i < filhos.length; i++) todos(filhos[i], nome, acc);
        return acc;
    }

    function criarProduto(nome, embalagens) {
        var p = App.novoProduto();
        p.nome = nome;
        p.categoriaId = App.categorias()[0].id;
        if (embalagens) p.embalagens = embalagens;
        verify(App.salvarProduto(p), App.ultimoErro());
        return App.buscarProdutosPorNome(nome)[0].produtoId;
    }

    function abrirEditor(pid) {
        var t = createTemporaryObject(cProdutos, palco, { width: 1280, height: 880 });
        verify(t !== null, cProdutos.errorString());
        t.abrirProduto(pid);
        findChild(t, "abasProduto").currentIndex = 1;   // Embalagens
        wait(0);
        return t;
    }

    // A linha nova trazia fator 1: caixinhas e fardos eram salvos como se
    // fossem uma unidade e vendiam assim por dias (o que houve na loja).
    function test_embalagem_nova_sem_fator_nao_salva() {
        var pid = criarProduto("Zzz Corr Sem Fator");
        var t = abrirEditor(pid);
        mouseClick(findChild(t, "adicionarEmbalagem"));
        wait(0);
        var nomes = todos(t, "nomeEmbalagem", []);
        var precos = todos(t, "precoEmbalagem", []);
        nomes[nomes.length - 1].text = "Caixinha";
        precos[precos.length - 1].text = "48,00";
        mouseClick(findChild(t, "salvarProduto"));
        wait(0);
        verify(findChild(t, "erroProduto").text.indexOf("Informe o fator") >= 0,
               findChild(t, "erroProduto").text);
        compare(App.embalagensDe(pid).length, 1, "a caixinha sem fator foi gravada");
    }

    // "4,5O" (letra O) virava R$ 0,00 e o produto saía de graça no PDV.
    function test_preco_ilegivel_nao_vira_zero() {
        var pid = criarProduto("Zzz Corr Preco", [
            { id: 0, nome: "Unidade", fator: 1, codigoBarras: "", preco: 450, custo: -1 }]);
        var t = abrirEditor(pid);
        todos(t, "precoEmbalagem", [])[0].text = "4,5O";
        mouseClick(findChild(t, "salvarProduto"));
        wait(0);
        verify(findChild(t, "erroProduto").text.indexOf("Preço inválido") >= 0,
               findChild(t, "erroProduto").text);
        compare(App.embalagensDe(pid)[0].preco, 450);
    }

    // Salvar o produto pela tela gravava "sem custo" por cima do custo de
    // compra das embalagens.
    function test_salvar_pela_tela_preserva_custo_de_compra() {
        var pid = criarProduto("Zzz Corr Custo", [
            { id: 0, nome: "Unidade", fator: 1, codigoBarras: "", preco: 450, custo: 300 }]);
        compare(App.embalagensDe(pid)[0].custo, 300);
        var t = abrirEditor(pid);
        mouseClick(findChild(t, "salvarProduto"));
        wait(0);
        compare(App.embalagensDe(pid)[0].custo, 300);
    }

    // Produto em ml: o custo por ml truncado em centavos fazia a Compra sugerir
    // R$ 10,00 para a garrafa de 1 L que custou R$ 18,99 (Black Stone na loja).
    function test_compra_sugere_custo_exato_em_ml() {
        var p = App.novoProduto();
        p.nome = "Zzz Corr Whisky";
        p.categoriaId = App.categorias()[0].id;
        p.unidadeBase = "ml";
        p.embalagens = [{ id: 0, nome: "Garrafa", fator: 1000, codigoBarras: "", preco: 9000, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());
        var pid = App.buscarProdutosPorNome(p.nome)[0].produtoId;
        var emb = App.embalagensDe(pid)[0].id;
        verify(App.registrarEntrada(pid, emb, 1, "18,99", "", "", ""), App.ultimoErro());

        var t = createTemporaryObject(cCompras, palco, { width: 1160, height: 740 });
        var dlg = findChild(t, "novaCompraDialog");
        dlg.abrir();
        dlg.adicionarProduto({ produtoId: pid, nome: p.nome, embalagemId: emb, fator: 1000 });
        wait(0);
        compare(findChild(dlg.contentItem, "custoItemCompra").text, "18,99");
        dlg.close();
    }

    // "Desativar" agia no clique: o cliente sumia da lista, e se devia, a
    // dívida ficava no Financeiro sem aparecer em Clientes.
    function test_desativar_cliente_pede_confirmacao() {
        var c = App.novoCliente();
        c.nome = "Zzz Corr Cliente";
        verify(App.salvarCliente(c), App.ultimoErro());
        var id = 0;
        var lista = App.clientesLista();
        for (var i = 0; i < lista.length; i++) if (lista[i].nome === c.nome) id = lista[i].id;
        verify(id > 0);

        var t = createTemporaryObject(cClientes, palco, { width: 1280, height: 880 });
        t.abrirCliente(id);
        wait(0);
        mouseClick(findChild(t, "desativarCliente"));
        wait(0);
        verify(findChild(t, "confirmarDesativarCliente").opened, "não pediu confirmação");
        var ainda = false;
        lista = App.clientesLista();
        for (var j = 0; j < lista.length; j++) if (lista[j].id === id) ainda = true;
        verify(ainda, "desativou sem confirmar");
        findChild(t, "confirmarDesativarCliente").close();
    }
}
