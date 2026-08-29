import QtQuick
import QtTest
import Distribuidora

// Perfil "Funcionário": o dono precisa poder confiar no que o perfil diz.
// Antes havia chave declarada que ninguém lia — o perfil dizia
// "pode_dar_desconto: false" e o funcionário dava desconto à vontade. Este caso
// entra de fato como funcionário e confere cada trava, na tela e no backend.
TestCase {
    id: caso
    name: "Permissoes"
    width: 1100
    height: 700
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cPdv;      PdvScreen      {} }
    Component { id: cProdutos; ProdutosScreen {} }
    Component { id: cEstoque;  EstoqueScreen  {} }
    Component { id: cSidebar;  Sidebar        {} }

    readonly property string loginFunc: "func.teste"
    readonly property string senhaFunc: "func12345"

    function initTestCase() {
        // Cria o funcionário (perfil 2) enquanto ainda somos administrador.
        var u = App.novoUsuario();
        u.nome = "Funcionario Teste";
        u.login = loginFunc;
        u.perfilId = 2;
        verify(App.salvarUsuario(u, senhaFunc), "não criou o funcionário: " + App.ultimoErro());
    }

    function cleanupTestCase() {
        // Devolve a sessão ao administrador para os outros casos.
        App.logout();
        App.login("teste", "teste1234");
    }

    function entrarComoFuncionario() {
        App.logout();
        verify(App.login(loginFunc, senhaFunc), "o funcionário não conseguiu entrar");
    }

    // --- O que o perfil promete ---------------------------------------------

    function test_perfil_habilita_o_balcao() {
        entrarComoFuncionario();
        verify(App.temPermissao("vende"),              "funcionário tem que vender");
        verify(App.temPermissao("consulta_produtos"),  "tem que consultar preço");
        verify(App.temPermissao("recebe_mercadoria"),  "tem que dar entrada de mercadoria");
        verify(App.temPermissao("atende_cliente"),     "tem que atender cliente/fiado");
    }

    function test_perfil_bloqueia_a_retaguarda() {
        entrarComoFuncionario();
        verify(!App.temPermissao("edita_produto"));
        verify(!App.temPermissao("pode_dar_desconto"));
        verify(!App.temPermissao("ajusta_estoque"));
        verify(!App.temPermissao("ve_relatorios"));
        verify(!App.temPermissao("ve_financeiro"));
        verify(!App.temPermissao("pode_cancelar_venda"));
        verify(!App.temPermissao("gerencia_usuarios"));
    }

    // --- E o que o sistema faz de verdade ------------------------------------

    // A trava não pode viver só na tela: tem que recusar no backend também.
    function test_backend_recusa_o_que_o_perfil_proibe() {
        entrarComoFuncionario();

        var p = App.novoProduto();
        p.nome = "Zzz Proibido";
        p.categoriaId = App.categorias()[0].id;
        verify(!App.salvarProduto(p), "funcionário não pode cadastrar produto");
        verify(App.ultimoErro().length > 0, "e tem que dizer por quê");

        verify(!App.registrarInventario(1, 999, "teste"), "não pode ajustar inventário");
        verify(!App.registrarRetirada(1, 1, 1, "teste"),  "não pode retirar do estoque");

        var r = App.cancelarVenda(1, "teste");
        compare(r.ok, false, "não pode cancelar venda");
    }

    // Desconto: some da tela E é recusado na finalização.
    function test_desconto_fora_do_alcance() {
        entrarComoFuncionario();

        var pdv = createTemporaryObject(cPdv, palco, { width: 1060, height: 660 });
        verify(pdv !== null, cPdv.errorString());
        wait(0);
        compare(pdv.podeDarDesconto, false, "o campo de desconto tem que sumir");

        // Mesmo forçando o desconto por fora da tela, a venda é recusada.
        var venda = App.finalizarVenda({ desconto: 500, clienteId: 0, itens: [], pagamentos: [] });
        compare(venda.ok, false);
        verify(("" + venda.erro).length > 0, "a recusa tem que explicar o motivo");
    }

    // Telas: o que ele não pode usar não fica ligado nem visível.
    function test_telas_refletem_o_perfil() {
        entrarComoFuncionario();

        var prod = createTemporaryObject(cProdutos, palco, { width: 1060, height: 660 });
        verify(prod !== null, cProdutos.errorString());
        wait(0);
        compare(prod.podeEditar, false, "Produtos tem que ficar em consulta");

        var est = createTemporaryObject(cEstoque, palco, { width: 1060, height: 660 });
        verify(est !== null, cEstoque.errorString());
        wait(0);
        compare(est.podeAjustarEstoque, false, "sem inventário/retirada");
        compare(est.podeReceberMercadoria, true, "mas recebe mercadoria");
    }

    // Barra lateral: retaguarda não aparece para o funcionário.
    function test_sidebar_esconde_a_retaguarda() {
        entrarComoFuncionario();

        var bar = createTemporaryObject(cSidebar, palco, { width: 240, height: 660 });
        verify(bar !== null, cSidebar.errorString());
        wait(0);
        verify(bar.temPerm("vende"),               "PDV tem que aparecer");
        verify(!bar.temPerm("ve_relatorios"),      "Relatórios não pode aparecer");
        verify(!bar.temPerm("ve_financeiro"),      "Compras/Financeiro não podem aparecer");
        verify(!bar.temPerm("gerencia_usuarios"),  "Usuários/Backup não podem aparecer");
    }

    // O administrador continua passando por tudo (chave "tudo").
    function test_admin_continua_com_tudo() {
        App.logout();
        verify(App.login("teste", "teste1234"));
        verify(App.temPermissao("edita_produto"));
        verify(App.temPermissao("pode_dar_desconto"));
        verify(App.temPermissao("ajusta_estoque"));
        verify(App.temPermissao("ve_relatorios"));
        verify(App.temPermissao("gerencia_usuarios"));
    }
}
