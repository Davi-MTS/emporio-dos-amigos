import QtQuick
import QtTest
import Distribuidora

// Fila de fotos em lote.
//
// O valor da tela está inteiro no ritmo: atribuiu → grava → anda → limpa o
// campo → devolve o foco. Se qualquer elo desse encadeamento quebrar, quem
// está cadastrando 200 produtos só descobre depois de ter atribuído dezenas de
// fotos ao produto errado.
TestCase {
    id: caso
    name: "FotosEmLote"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    // O diálogo se dimensiona a partir do PAI. Para provar que ele cabe numa
    // janela restaurada é preciso um pai daquele tamanho — passar width/height
    // direto no diálogo mataria a conta que se quer testar.
    Component { id: cJanela; Item {} }
    Component { id: cFila; FotosEmLoteDialog {} }
    Component { id: cProdutos; ProdutosScreen {} }

    property int idA: 0
    property int idB: 0
    property string codigoB: "7899000012345"

    function initTestCase() {
        var cat = App.categorias()[0].id;

        var a = App.novoProduto();
        a.nome = "Zzz Foto Alfa";
        a.categoriaId = cat;
        verify(App.salvarProduto(a), App.ultimoErro());

        var b = App.novoProduto();
        b.nome = "Zzz Foto Beta";
        b.categoriaId = cat;
        b.embalagens = [{ id: 0, nome: "Unidade", fator: 1,
                          codigoBarras: codigoB, preco: 500, custo: -1 }];
        verify(App.salvarProduto(b), App.ultimoErro());

        idA = App.buscarProdutosPorNome("Zzz Foto Alfa")[0].produtoId;
        idB = App.buscarProdutosPorNome("Zzz Foto Beta")[0].produtoId;
        verify(idA > 0 && idB > 0);
    }

    function limparFotos() {
        App.removerFotoProduto(idA);
        App.removerFotoProduto(idB);
    }

    function abrirFila(largura, altura) {
        var janela = createTemporaryObject(cJanela, palco,
                                           { width: largura || 1000, height: altura || 700 });
        verify(janela !== null, cJanela.errorString());
        var d = createTemporaryObject(cFila, janela);
        verify(d !== null, cFila.errorString());
        d.open();
        wait(0);
        d.reiniciar();
        d.comecar(FotosDeTeste);
        wait(0);
        return d;
    }

    // ------------------------------------------------------------------ fila
    function test_fila_recebe_os_arquivos() {
        limparFotos();
        var d = abrirFila();
        compare(d.fila.length, 3, "os três arquivos deveriam ter entrado");
        compare(d.indice, 0);
        verify(d.temFila);
    }

    // Arquivo que não é imagem não pode entrar na fila e virar uma parada seca
    // no meio do trabalho.
    function test_fila_ignora_o_que_nao_e_imagem() {
        limparFotos();
        var d = abrirFila();
        var antes = d.fila.length;
        d.comecar(["file:///tmp/planilha.xlsx", "file:///tmp/nota.pdf"]);
        compare(d.fila.length, antes, "arquivo que não é imagem entrou na fila");
        verify(d.erro.length > 0, "deveria explicar que não havia imagem");
    }

    // ------------------------------------------------------------- atribuir
    function test_atribuir_grava_e_anda() {
        limparFotos();
        var d = abrirFila();

        verify(!App.produtoTemFoto(idA));
        d.atribuir(idA, false);
        wait(0);

        verify(App.produtoTemFoto(idA), "a foto não foi gravada");
        compare(d.indice, 1, "não andou para a próxima foto");
        compare(d.atribuidas, 1);
        compare(d.erro, "", "não deveria haver erro");
    }

    // Bipar o código de barras é o caminho rápido: o leitor digita e dá Enter.
    function test_codigo_de_barras_atribui_direto() {
        limparFotos();
        var d = abrirFila();

        var campo = findChild(d, "buscaFoto");
        verify(campo !== null, "campo de busca não encontrado");
        campo.text = codigoB;
        d.confirmarBusca();
        wait(0);

        verify(App.produtoTemFoto(idB), "bipar o código não atribuiu a foto");
        compare(d.indice, 1);
        compare(campo.text, "", "o campo tem que voltar vazio para o próximo bipe");
    }

    // Digitar o nome tem que achar o produto e trazer junto se ele já tem foto
    // — é o aviso que evita sobrescrever a foto certa sem perceber.
    function test_busca_por_nome_marca_quem_ja_tem_foto() {
        limparFotos();
        var d = abrirFila();

        d.buscar("Zzz Foto Alfa");
        compare(d.candidatos.length, 1);
        compare(d.candidatos[0].temFoto, false);

        d.atribuir(idA, false);
        wait(0);

        d.buscar("Zzz Foto Alfa");
        compare(d.candidatos.length, 1);
        compare(d.candidatos[0].temFoto, true,
                "depois de atribuir, a busca tem que dizer que já tem foto");
    }

    // Substituir não pode acontecer no susto: abre confirmação e NÃO grava.
    function test_produto_com_foto_pede_confirmacao() {
        limparFotos();
        var d = abrirFila();
        d.atribuir(idA, false);
        wait(0);
        compare(d.indice, 1);

        d.atribuir(idA, true);
        wait(0);
        compare(d.indice, 1, "não pode andar antes de o dono confirmar a troca");
        compare(d.atribuidas, 1, "não pode contar como atribuída sem confirmação");
    }

    // ---------------------------------------------------------------- pular
    function test_pular_nao_grava_nada() {
        limparFotos();
        var d = abrirFila();

        d.pular();
        wait(0);

        compare(d.indice, 1);
        compare(d.puladas, 1);
        compare(d.atribuidas, 0);
        verify(!App.produtoTemFoto(idA));
        verify(!App.produtoTemFoto(idB));
    }

    // -------------------------------------------------------------- desfazer
    function test_desfazer_tira_a_foto_e_volta() {
        limparFotos();
        var d = abrirFila();

        d.atribuir(idA, false);
        wait(0);
        verify(App.produtoTemFoto(idA));

        d.desfazer();
        wait(0);

        verify(!App.produtoTemFoto(idA), "desfazer deixou a foto no produto errado");
        compare(d.indice, 0, "desfazer tem que voltar para a mesma foto");
        compare(d.atribuidas, 0);
    }

    // Desfazer uma SUBSTITUIÇÃO devolveria a foto antiga, que não foi guardada.
    // Em vez de mentir, o botão fica desligado.
    function test_desfazer_desligado_apos_substituir() {
        limparFotos();
        var d = abrirFila();

        d.atribuir(idA, false);
        wait(0);

        // Simula o caminho da confirmação de troca.
        d.ultimoProdutoId = idA;
        d.ultimoSubstituiu = true;

        var botao = findChild(d, "desfazerFoto");
        verify(botao !== null, "botão de desfazer não encontrado");
        compare(botao.enabled, false, "desfazer não pode se oferecer para o que não desfaz");

        d.desfazer();
        verify(App.produtoTemFoto(idA), "desfazer agiu mesmo estando desligado");
    }

    // ------------------------------------------------------------- contagem
    function test_contador_de_sem_foto_diminui() {
        limparFotos();
        var d = abrirFila();

        var antes = App.contarProdutosSemFoto();
        verify(antes > 0);

        d.atribuir(idA, false);
        wait(0);

        compare(App.contarProdutosSemFoto(), antes - 1,
                "o contador de produtos sem foto não acompanhou");
    }

    // ------------------------------------------------- ligacao com a tela
    // O diálogo mora no overlay da janela. Se essa ligação quebrar, o botão
    // "Fotos em lote" não abre nada e não há erro nenhum na tela — o pior
    // jeito de um recurso sumir.
    function test_botao_da_tela_abre_a_fila() {
        var tela = createTemporaryObject(cProdutos, palco, { width: 1160, height: 740 });
        verify(tela !== null, cProdutos.errorString());
        wait(0);

        var botao = findChild(tela, "botaoFotosLote");
        verify(botao !== null, "botão 'Fotos em lote' não está na tela de Produtos");
        verify(botao.enabled, "o admin tem que poder abrir a fila");

        verify(!tela.filaDeFotosAberta());
        botao.clicked();
        wait(0);
        verify(tela.filaDeFotosAberta(), "o botão não abriu a fila de fotos");

        var filtro = findChild(tela, "filtroSemFoto");
        verify(filtro !== null, "filtro 'Sem foto' não está na tela");
    }

    // ------------------------------------------------------------ tamanhos
    // Janela restaurada é o tamanho em que o dono realmente usa. Nada pode
    // começar fora dela nem ser espremido a zero.
    function test_cabe_na_janela_restaurada() {
        limparFotos();
        var d = abrirFila(760, 560);
        wait(0);

        verify(d.width > 0 && d.width <= 760,
               "o diálogo ficou mais largo que a janela: " + d.width);
        verify(d.height > 0 && d.height <= 560,
               "o diálogo ficou mais alto que a janela: " + d.height);

        var campo = findChild(d, "buscaFoto");
        verify(campo !== null);
        verify(campo.width > 0, "o campo de busca foi espremido a zero");

        var lista = findChild(d, "candidatosFoto");
        verify(lista !== null);
        verify(lista.width > 0, "a lista de candidatos foi espremida a zero");
    }
}
