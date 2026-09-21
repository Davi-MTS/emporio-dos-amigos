import QtQuick
import QtTest
import Distribuidora

// Margem na tela de Estoque, e o filtro por faixa de margem.
//
// O número é margem sobre o PREÇO DE VENDA, não markup: custo R$ 10,00 e venda
// R$ 15,00 dá 33,3%, não 50%. Confundir os dois faz o dono achar que ganha mais
// do que ganha — por isso o valor exato está no teste, e não só "apareceu algo".
//
// As faixas são decisão do dono: abaixo de 35% baixa, de 35% a 45% boa, acima
// de 45% muito boa. As BORDAS (35,0% e 45,0%) estão cobertas de propósito — é
// onde "de 35 a 45" vira código e onde um `<` no lugar de `<=` passa batido.
//
// Os dois casos que não viram número: produto sem preço de venda e produto com
// custo desconhecido (0). Nos dois a tela mostra "—" e eles não entram em faixa
// nenhuma.
TestCase {
    id: caso
    name: "EstoqueMargem"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cEstoque; EstoqueScreen {} }

    readonly property string prefixo: "Zzz Margem"
    property int idNormal: 0
    property int idSemPreco: 0
    property int idSemCusto: 0
    property int idPrejuizo: 0

    function criar(sufixo, preco, custo) {
        var p = App.novoProduto();
        p.nome = prefixo + " " + sufixo;
        p.categoriaId = App.categorias()[0].id;
        p.embalagens = [{ id: 0, nome: "Unidade", fator: 1, codigoBarras: "",
                          preco: preco, custo: -1 }];
        verify(App.salvarProduto(p), App.ultimoErro());
        var id = App.buscarProdutosPorNome(p.nome)[0].produtoId;
        if (custo !== "") {
            var emb = App.embalagensDe(id)[0].id;
            verify(App.registrarEntrada(id, emb, 10, custo, "", "", ""), App.ultimoErro());
        }
        return id;
    }

    function initTestCase() {
        idNormal   = criar("Normal", 1500, "10,00");    // 33,3%  -> baixa
        idSemPreco = criar("Sem Preco", 0, "10,00");    // sem preço -> "—"
        idSemCusto = criar("Sem Custo", 1500, "");      // custo 0   -> "—"
        idPrejuizo = criar("Prejuizo", 450, "6,00");    // -33,3% -> baixa
        criar("Quase Boa", 1000, "6,51");               // 34,9%  -> baixa
        criar("Boa Borda Baixa", 1000, "6,50");         // 35,0%  -> boa
        criar("Boa Borda Alta", 1000, "5,50");          // 45,0%  -> boa
        criar("Muito Boa", 1000, "5,00");               // 50,0%  -> muito boa
    }

    function abrir() {
        var t = createTemporaryObject(cEstoque, palco, { width: 1160, height: 740 });
        verify(t !== null, cEstoque.errorString());
        App.recarregarEstoque(prefixo);
        wait(0);
        return t;
    }

    function cleanup() {
        App.estoque.filtroMargem = "";
        App.estoque.filtroStatus = "";
        App.recarregarEstoque("");
    }

    // Qt.UserRole (256) + posição do role no enum do EstoqueListModel, que é
    // onde MargemRole entrou (no fim, de propósito).
    readonly property int roleMargem: 257 + 9
    readonly property int roleNome: 257 + 1

    function margemDe(nome) {
        for (var i = 0; i < App.estoque.rowCount(); i++) {
            var idx = App.estoque.index(i, 0);
            if (App.estoque.data(idx, roleNome) === prefixo + " " + nome)
                return App.estoque.data(idx, roleMargem);
        }
        return "não achei " + nome;
    }

    function test_margem_e_sobre_o_preco_de_venda() {
        abrir();
        compare(App.estoque.rowCount(), 8);
        compare(margemDe("Normal"), 333, "custo 10 e venda 15 é 33,3% (markup seria 50%)");
        compare(margemDe("Prejuizo"), -333, "custo acima do preço sai negativo");
        compare(margemDe("Sem Preco"), undefined);
        compare(margemDe("Sem Custo"), undefined);
    }

    // As bordas da faixa "boa": 35,0% e 45,0% estão DENTRO dela.
    function test_faixas_nas_bordas() {
        abrir();
        App.estoque.filtroMargem = "boa";
        wait(0);
        var nomes = [];
        for (var i = 0; i < App.estoque.rowCount(); i++)
            nomes.push(App.estoque.data(App.estoque.index(i, 0), roleNome));
        compare(nomes.length, 2, "35,0% e 45,0% são as duas 'boas': " + nomes);
        verify(nomes.indexOf(prefixo + " Boa Borda Baixa") >= 0, "35,0% tem que ser boa");
        verify(nomes.indexOf(prefixo + " Boa Borda Alta") >= 0, "45,0% tem que ser boa");

        App.estoque.filtroMargem = "muitoboa";
        wait(0);
        compare(App.estoque.rowCount(), 1, "só 50% é muito boa");
        compare(App.estoque.data(App.estoque.index(0, 0), roleNome), prefixo + " Muito Boa");

        App.estoque.filtroMargem = "baixa";
        wait(0);
        compare(App.estoque.rowCount(), 3, "33,3%, 34,9% e o prejuízo");
    }

    // Quem não tem margem não entra em faixa nenhuma: só aparece em "Todas".
    function test_sem_margem_fica_fora_das_faixas() {
        abrir();
        var c = App.estoque.contagemMargem;
        compare(c.todos, 8);
        compare(c.baixa, 3);
        compare(c.boa, 2);
        compare(c.muitoboa, 1);
        compare(c.sem, 2, "sem preço e sem custo não são 'baixa'");

        var faixas = ["baixa", "boa", "muitoboa"];
        for (var i = 0; i < faixas.length; i++) {
            App.estoque.filtroMargem = faixas[i];
            wait(0);
            for (var j = 0; j < App.estoque.rowCount(); j++) {
                var nome = App.estoque.data(App.estoque.index(j, 0), roleNome);
                verify(nome.indexOf("Sem Preco") < 0 && nome.indexOf("Sem Custo") < 0,
                       nome + " não devia estar na faixa " + faixas[i]);
            }
        }
    }

    // Filtro desconhecido mostra tudo, nunca uma lista vazia sem explicação.
    function test_faixa_invalida_mostra_tudo() {
        abrir();
        App.estoque.filtroMargem = "excelente";
        compare(App.estoque.filtroMargem, "");
        compare(App.estoque.rowCount(), 8);
    }

    // Os dois filtros valem juntos, e cada contagem é sobre o que o OUTRO
    // filtro deixou passar — o número do botão tem que ser o que se encontra
    // ao clicar nele.
    function test_filtros_se_combinam() {
        abrir();
        App.estoque.filtroStatus = "ok";        // todos têm estoque
        wait(0);
        compare(App.estoque.contagemMargem.boa, 2);

        App.estoque.filtroMargem = "boa";
        wait(0);
        compare(App.estoque.rowCount(), 2);
        compare(App.estoque.contagem.todos, 2, "a contagem por situação segue a faixa");

        App.estoque.filtroStatus = "zerado";    // nenhum destes está zerado
        wait(0);
        compare(App.estoque.rowCount(), 0);
        compare(App.estoque.contagemMargem.boa, 0, "nenhuma 'boa' está zerada");
    }

    // O botão da tela liga o filtro de verdade, e mostra quantos há.
    function test_botao_da_tela_filtra() {
        var tela = abrir();
        var seg = findChild(tela, "filtroMargemEstoque");
        verify(seg !== null, "o filtro de margem não está na tela");
        compare(seg.options.length, 4);
        verify(seg.options[1].indexOf("Baixa") === 0, seg.options[1]);
        verify(seg.options[1].indexOf("3") > 0, "o botão mostra a contagem: " + seg.options[1]);

        seg.currentIndex = 3;   // Muito boa
        wait(0);
        compare(App.estoque.filtroMargem, "muitoboa");
        compare(App.estoque.rowCount(), 1);
    }

    // Sair da tela desliga os dois filtros: o model é um só para o app.
    function test_sair_da_tela_desliga_o_filtro() {
        var t = abrir();
        App.estoque.filtroMargem = "baixa";
        t.destroy();
        wait(0);
        compare(App.estoque.filtroMargem, "");
    }

    // A coluna existe e mostra o texto formatado em pt-BR, não o número cru.
    function test_coluna_da_tela_mostra_a_margem() {
        var tela = abrir();
        var lista = findChild(tela, "listaEstoque");
        verify(lista !== null, "a lista de estoque não está na tela");
        compare(lista.count, 8);
        verify(tela.podeVerMargem, "o administrador vê a margem");
        verify(findChild(tela, "margemCabecalho").visible);

        var vistos = {};
        for (var i = 0; i < lista.count; i++) {
            var item = lista.itemAtIndex(i);
            verify(item !== null, "linha " + i + " não foi criada");
            var celula = findChild(item, "margemLinha");
            verify(celula !== null, "a coluna Margem não está na linha");
            vistos[item.nome] = celula.text;
        }

        compare(vistos[prefixo + " Normal"], "33,3%");
        compare(vistos[prefixo + " Prejuizo"], "-33,3%");
        compare(vistos[prefixo + " Muito Boa"], "50,0%");
        compare(vistos[prefixo + " Sem Preco"], "—");
        compare(vistos[prefixo + " Sem Custo"], "—");
    }

    // A coluna Margem NÃO pode falar a mesma língua de cor do selo de Status,
    // que fica colado nela: verde/âmbar nos dois fazia ler os dois como a mesma
    // informação. Só o prejuízo é colorido.
    function test_margem_nao_usa_as_cores_do_selo() {
        var tela = abrir();
        compare(tela.corDaMargem(undefined) + "", Theme.textMuted + "");
        compare(tela.corDaMargem(-333) + "", Theme.danger + "", "prejuízo em vermelho");
        var faixas = [349, 350, 450, 451, 900];
        for (var i = 0; i < faixas.length; i++) {
            compare(tela.corDaMargem(faixas[i]) + "", Theme.text + "",
                    faixas[i] + " tem que sair em texto normal");
            verify(tela.corDaMargem(faixas[i]) + "" !== Theme.success + "");
            verify(tela.corDaMargem(faixas[i]) + "" !== Theme.warning + "");
        }
    }

    // A faixa vem escrita embaixo do número, e segue a MESMA regra do filtro:
    // filtrar "Baixa" e ler "boa" na linha seria a tela se contradizendo.
    function test_faixa_escrita_bate_com_o_filtro() {
        var tela = abrir();
        compare(tela.nomeDaFaixa(undefined), "");
        compare(tela.nomeDaFaixa(-333), "prejuízo");
        compare(tela.nomeDaFaixa(349), "baixa");
        compare(tela.nomeDaFaixa(350), "boa");
        compare(tela.nomeDaFaixa(450), "boa");
        compare(tela.nomeDaFaixa(451), "muito boa");

        // E o que está escrito na linha bate com a faixa filtrada.
        App.estoque.filtroMargem = "muitoboa";
        wait(0);
        var lista = findChild(tela, "listaEstoque");
        compare(lista.count, 1);
        compare(findChild(lista.itemAtIndex(0), "faixaLinha").text, "muito boa");
        compare(findChild(lista.itemAtIndex(0), "margemLinha").text, "50,0%");
    }

    // Fio entre as linhas, como nas Compras.
    function test_linhas_separadas_por_um_fio() {
        var tela = abrir();
        var lista = findChild(tela, "listaEstoque");
        var fio = findChild(lista.itemAtIndex(0), "separadorLinha");
        verify(fio !== null, "faltou o fio entre as linhas");
        compare(fio.height, 1);
        compare(fio.color + "", Theme.border + "", "o fio tem que ser suave");
    }

    // A cerca entre Margem e Status existe — é ela que separa as duas leituras.
    function test_divisor_entre_margem_e_status() {
        var tela = abrir();
        verify(findChild(tela, "divisorStatus") !== null,
               "faltou o divisor entre os números e o selo de situação");
    }

    // AS COLUNAS DA LINHA TÊM DE BATER COM AS DO CABEÇALHO — na tela larga, que
    // é onde sobra folga para uma coluna gulosa engolir.
    //
    // Foi assim que a coluna da margem quebrou a lista: uma Layout dentro de
    // outra tem `Layout.fillWidth` TRUE por padrão (item comum tem false), então
    // ela pegou 293 px em vez de 104 e empurrou Qtd, Mínimo e Custo ~380 px para
    // longe do próprio cabeçalho.
    function test_colunas_batem_com_o_cabecalho() {
        var tela = createTemporaryObject(cEstoque, palco, { width: 1660, height: 740 });
        verify(tela !== null, cEstoque.errorString());
        App.recarregarEstoque(prefixo);
        wait(0);

        var lista = findChild(tela, "listaEstoque");
        var item = lista.itemAtIndex(0);
        verify(item !== null);

        var colunas = [["cabQtd", "qtdLinha"], ["cabMinimo", "minimoLinha"],
                       ["cabCusto", "custoLinha"], ["margemCabecalho", "margemLinha"]];
        for (var i = 0; i < colunas.length; i++) {
            var cab = findChild(tela, colunas[i][0]);
            var cel = findChild(item, colunas[i][1]);
            verify(cab !== null && cel !== null, "não achei " + colunas[i]);
            // Todas são alinhadas à direita: a borda direita é o que tem de
            // coincidir. A foto do produto desloca a linha em 40 px.
            var dCab = Math.round(cab.mapToItem(tela, 0, 0).x + cab.width);
            var dCel = Math.round(cel.mapToItem(tela, 0, 0).x + cel.width);
            verify(Math.abs(dCab - dCel) <= 2,
                   colunas[i][0] + " termina em " + dCab + " e a linha em " + dCel);
        }
    }

    // O diálogo do produto mostra a margem E o preço de onde ela saiu — sem o
    // preço ao lado, a porcentagem é um número sem origem.
    function test_dialogo_mostra_margem_com_o_preco() {
        var tela = abrir();
        tela.abrirMov(idNormal);
        wait(0);

        var dlg = findChild(tela, "movDialog");
        verify(dlg !== null);
        compare(dlg.atual.precoBase, 1500);
        compare(dlg.atual.margem, 333);

        var bloco = findChild(dlg, "margemDialogo");
        verify(bloco !== null, "o bloco de margem não está no diálogo");
        verify(bloco.visible, "com preço e custo conhecidos a margem tem que aparecer");
        // No diálogo a faixa vem junto: ali sobra espaço e o nome tira a dúvida
        // de "33,3% é bom ou ruim?" sem obrigar a decorar os limites.
        compare(findChild(dlg, "margemValor").text, "33,3% · baixa");
    }

    // Sem custo conhecido o bloco some — melhor não dizer nada do que dizer 100%.
    function test_dialogo_esconde_a_margem_sem_custo() {
        var tela = abrir();
        tela.abrirMov(idSemCusto);
        wait(0);

        var dlg = findChild(tela, "movDialog");
        compare(dlg.atual.margem, undefined);
        verify(!findChild(dlg, "margemDialogo").visible);
    }
}
