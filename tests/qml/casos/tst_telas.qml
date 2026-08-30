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
