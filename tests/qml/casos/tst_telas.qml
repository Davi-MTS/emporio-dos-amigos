import QtQuick
import QtTest
import Distribuidora

// Toda tela do sistema tem que ABRIR — em tela cheia e com a janela restaurada.
// Um erro de QML (propriedade que não existe, id trocado, tipo faltando) não
// quebra a compilação: só aparece quando alguém abre aquela aba, às vezes no
// meio do expediente. Este caso abre as 12 telas nos dois tamanhos.
TestCase {
    id: caso
    name: "Telas"
    width: 1366
    height: 768
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }

    Component { id: cDashboard;  DashboardScreen  {} }
    Component { id: cProdutos;   ProdutosScreen   {} }
    Component { id: cEstoque;    EstoqueScreen    {} }
    Component { id: cVencimento; VencimentoScreen {} }
    Component { id: cPdv;        PdvScreen        {} }
    Component { id: cCaixa;      CaixaScreen      {} }
    Component { id: cLogin;      LoginScreen      {} }
    Component { id: cUsuarios;   UsuariosScreen   {} }
    Component { id: cCompras;    ComprasScreen    {} }
    Component { id: cClientes;   ClientesScreen   {} }
    Component { id: cFinanceiro; FinanceiroScreen {} }
    Component { id: cRelatorios; RelatoriosScreen {} }
    Component { id: cVendas;     VendasScreen     {} }
    Component { id: cBackup;     BackupScreen     {} }

    function abrir(comp, larg, alt) {
        var obj = createTemporaryObject(comp, palco,
                                        { width: larg, height: alt });
        verify(obj !== null, "não instanciou: " + comp.errorString());
        wait(0);            // deixa Component.onCompleted e os bindings rodarem
        waitForRendering(obj);
        return obj;
    }

    function test_abre_data() {
        return [
            { tag: "Dashboard",  comp: cDashboard  },
            { tag: "Produtos",   comp: cProdutos   },
            { tag: "Estoque",    comp: cEstoque    },
            { tag: "Vencimento", comp: cVencimento },
            { tag: "PDV",        comp: cPdv        },
            { tag: "Caixa",      comp: cCaixa      },
            { tag: "Login",      comp: cLogin      },
            { tag: "Usuarios",   comp: cUsuarios   },
            { tag: "Compras",    comp: cCompras    },
            { tag: "Clientes",   comp: cClientes   },
            { tag: "Financeiro", comp: cFinanceiro },
            { tag: "Relatorios", comp: cRelatorios },
            { tag: "Vendas",     comp: cVendas     },
            { tag: "Backup",     comp: cBackup     }
        ];
    }

    // Erros de QML em tempo de execução saem como aviso e a tela fica quebrada
    // em silêncio. Aqui eles derrubam o teste.
    function marcarAvisosComoFalha() {
        failOnWarning(/is not a function/);
        failOnWarning(/is not defined/);
        failOnWarning(/Unable to assign/);
        failOnWarning(/Cannot read property/);
        failOnWarning(/TypeError/);
        failOnWarning(/ReferenceError/);
    }

    // Tela cheia (1366x768) — o tamanho normal de uso na loja.
    function test_abre(dados) {
        marcarAvisosComoFalha();
        var t = abrir(dados.comp, 1160, 700);
        verify(t.width > 0 && t.height > 0);
        naoEspremeConteudo(t, dados.tag);
    }

    // Janela restaurada (960x620): foi neste tamanho que a categoria passou por
    // cima do nome do produto. Nada pode transbordar da largura da tela.
    function test_abre_janela_restaurada_data() { return test_abre_data(); }
    function test_abre_janela_restaurada(dados) {
        marcarAvisosComoFalha();
        var t = abrir(dados.comp, 760, 560);
        verify(t.width > 0);
        naoTransborda(t, t.width, dados.tag);
    }

    // Entre a tela cheia e a janela restaurada existe um monte de tamanho
    // intermediário, e é lá que as coisas ficam feias sem ninguém ver: foi numa
    // janela de ~1400 que o dono achou os filtros do Estoque tortos, e a 760 as
    // colunas de Estoque escreviam "Produto" por cima de "Localização".
    //
    // Item dentro de um pai com `clip` é recortado DE PROPÓSITO (lista rolável),
    // então não conta.
    function dentroDeClip(item, raiz) {
        var at = item.parent;
        while (at && at !== raiz) {
            if (at.clip === true)
                return true;
            at = at.parent;
        }
        return false;
    }

    function passaDaBorda(raiz, largura, tag) {
        var fila = [raiz];
        while (fila.length > 0) {
            var it = fila.shift();
            for (var i = 0; i < it.children.length; i++) {
                var f = it.children[i];
                if (f.visible === false || f.width <= 0)
                    continue;
                var p = raiz.mapFromItem(f, 0, 0);
                if (!dentroDeClip(f, raiz) && p.x + f.width > largura + 2) {
                    fail(tag + " @" + largura + "px: " + f + " vai até x="
                         + Math.round(p.x + f.width) + ", "
                         + Math.round(p.x + f.width - largura) + "px fora da tela\n  "
                         + cadeia(f, raiz));
                }
                fila.push(f);
            }
        }
    }

    function test_cabe_em_qualquer_largura_data() { return test_abre_data(); }
    function test_cabe_em_qualquer_largura(dados) {
        // Sem `waitForRendering`: a geometria já está resolvida no polish, e
        // esperar o quadro em 14 telas × 4 larguras custava 4 minutos de suíte.
        var larguras = [1600, 1200, 900, 760];
        for (var i = 0; i < larguras.length; i++) {
            var t = createTemporaryObject(dados.comp, palco,
                                          { width: larguras[i], height: 620 });
            verify(t !== null, "não instanciou: " + dados.comp.errorString());
            wait(0);
            passaDaBorda(t, larguras[i], dados.tag);
            t.destroy();
            wait(0);
        }
    }

    // Descreve o item e seus pais (posição/largura) para dizer ONDE estourou.
    function cadeia(item, raiz) {
        var partes = [];
        var at = item;
        while (at && at !== raiz && partes.length < 8) {
            var p = raiz.mapFromItem(at, 0, 0);
            partes.push(("" + at).split("(")[0] + " x=" + Math.round(p.x)
                        + " w=" + Math.round(at.width));
            at = at.parent;
        }
        return partes.join("  ⊂  ");
    }

    // Conteúdo espremido: um layout cuja altura NATURAL é maior que a altura que
    // ele recebeu está com os filhos cortados. Foi assim que o "+ R$ 5,50" do
    // composto ficou pela metade — o card tinha 64 px cravados e o texto novo
    // não coube.
    function naoEspremeConteudo(raiz, tag) {
        var fila = [raiz];
        while (fila.length > 0) {
            var it = fila.shift();
            for (var i = 0; i < it.children.length; i++) {
                var f = it.children[i];
                if (f.visible === false)
                    continue;
                var ehLayout = ("" + f).indexOf("Layout") >= 0;
                if (ehLayout && f.height > 0 && f.implicitHeight > f.height + 1) {
                    fail(tag + ": " + f + " precisa de " + Math.ceil(f.implicitHeight)
                         + "px e recebeu " + Math.floor(f.height) + "px — conteúdo cortado
  "
                         + cadeia(f, raiz));
                }
                fila.push(f);
            }
        }
    }

    // Percorre a árvore procurando item visível que comece fora da direita da
    // tela — sintoma de coluna fixa que não coube.
    function naoTransborda(raiz, largura, tag) {
        var fila = [raiz];
        while (fila.length > 0) {
            var it = fila.shift();
            for (var i = 0; i < it.children.length; i++) {
                var f = it.children[i];
                if (f.visible === false)
                    continue;
                var p = raiz.mapFromItem(f, 0, 0);
                if (f.width > 0 && p.x >= largura + 1)
                    fail(tag + ": " + f + " começa em x=" + Math.round(p.x)
                         + " (largura da tela: " + largura + ")
  "
                         + cadeia(f, raiz));
                fila.push(f);
            }
        }
    }
}
