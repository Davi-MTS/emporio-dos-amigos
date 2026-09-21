import QtQuick
import QtTest
import Distribuidora

// Alinhamento da lista de Clientes.
//
// O selo de situação ("Em dia" / "Deve R$ x") ficava numa posição diferente em
// cada linha, colado no fim do nome: nome curto puxava o selo para a esquerda,
// nome longo empurrava para a direita. Numa lista de clientes isso vira uma
// escada e não dá para correr o olho pela coluna.
TestCase {
    id: caso
    name: "ClientesLista"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    Item { id: palco; anchors.fill: parent }
    Component { id: cClientes; ClientesScreen {} }

    readonly property string prefixo: "Zzz Lista"

    function criar(nome) {
        var c = App.novoCliente();
        c.nome = nome;
        verify(App.salvarCliente(c), App.ultimoErro());
    }

    function initTestCase() {
        // Nomes de larguras bem diferentes: é o que expõe o desalinhamento.
        criar(prefixo + " Ab");
        criar(prefixo + " Mulher do Paulo com nome bem comprido");
        criar(prefixo + " JOEL");
    }

    function abrir() {
        var t = createTemporaryObject(cClientes, palco, { width: 1160, height: 740 });
        verify(t !== null, cClientes.errorString());
        App.recarregarClientes(prefixo);
        wait(0);
        return t;
    }

    function cleanup() {
        App.recarregarClientes("");
    }

    // O selo começa no MESMO x em todas as linhas, e encostado à direita.
    function test_selo_de_situacao_alinhado_em_coluna() {
        var tela = abrir();
        var lista = findChild(tela, "listaClientes");
        verify(lista !== null, "a lista de clientes não está na tela");
        compare(lista.count, 3);

        var xs = [];
        var direitas = [];
        for (var i = 0; i < lista.count; i++) {
            var item = lista.itemAtIndex(i);
            verify(item !== null, "linha " + i + " não foi criada");
            var selo = findChild(item, "situacaoCliente");
            verify(selo !== null, "o selo de situação não está na linha");
            var p = selo.mapToItem(lista, 0, 0);
            xs.push(Math.round(p.x));
            direitas.push(Math.round(p.x + selo.width));
        }

        for (var j = 1; j < xs.length; j++)
            compare(xs[j], xs[0], "o selo tem que começar no mesmo x em toda linha");

        // E a coluna tem que ficar à direita, não no meio da tela.
        verify(direitas[0] > lista.width * 0.7,
               "o selo deveria encostar à direita da lista, está em " + direitas[0]
               + " de " + lista.width);
    }

    // Fio entre as linhas, como nas Compras.
    function test_linhas_separadas_por_um_fio() {
        var tela = abrir();
        var lista = findChild(tela, "listaClientes");
        var fio = findChild(lista.itemAtIndex(0), "separadorLinha");
        verify(fio !== null, "faltou o fio entre as linhas");
        compare(fio.height, 1);
        compare(fio.color + "", Theme.border + "", "o fio tem que ser suave");
    }
}
