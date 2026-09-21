#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"

class TstEstoqueRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void entradaAtualizaCustoMedioPonderado();
    void entradaSemCustoMantemCusto();
    void inventarioDefineQuantidade();
    void movimentacoesRegistradas();
    void retiradaBaixaEstoque();
    void margemSobreOPrecoDeVenda();
    void margemUsaAMenorEmbalagemComPreco();
    void margemSemPrecoOuSemCustoNaoExiste();
    void margemNegativaQuandoOCustoPassaOPreco();
    void margemEmMlNaoPerdeAFracaoDeCentavo();

private:
    // Produto novo a cada caso de margem: os outros casos compartilham
    // m_produtoId e mexer no preço dele quebraria a sequência de custo médio.
    int criarProduto(const QString &nome, const QVector<QPair<int, qint64>> &fatorPreco,
                     const QString &unidadeBase = QStringLiteral("unidade"));

    QTemporaryDir m_dir;
    Database m_db;
    int m_produtoId = 0;

    EstoqueRepository estoque() { return EstoqueRepository(m_db.connection()); }
};

void TstEstoqueRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))),
             qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    // Cria um produto (com embalagem base) para ter linha de estoque.
    ProdutoRepository prepo(m_db.connection());
    Produto p;
    p.nome = QStringLiteral("Produto Teste");
    p.estoqueMinimo = 10;
    Embalagem base;
    base.nome = QStringLiteral("Unidade");
    base.fator = 1;
    base.precoVenda = 500;
    p.embalagens = {base};
    QVERIFY2(prepo.salvar(p), qUtf8Printable(prepo.ultimoErro()));
    m_produtoId = p.id;
    QVERIFY(m_produtoId > 0);
}

void TstEstoqueRepository::entradaAtualizaCustoMedioPonderado()
{
    auto r = estoque();

    // 100 un a 2,50 -> custo 250.
    QVERIFY2(r.registrarEntrada(m_produtoId, 100, 250, 0, QStringLiteral("compra 1")),
             qUtf8Printable(r.ultimoErro()));
    ItemEstoque it = r.item(m_produtoId);
    QCOMPARE(it.quantidade, qint64(100));
    QCOMPARE(it.custoMedio, qint64(250));

    // +100 un a 3,50 -> média ponderada (100*250 + 100*350)/200 = 300.
    QVERIFY2(r.registrarEntrada(m_produtoId, 100, 350, 0, QStringLiteral("compra 2")),
             qUtf8Printable(r.ultimoErro()));
    it = r.item(m_produtoId);
    QCOMPARE(it.quantidade, qint64(200));
    QCOMPARE(it.custoMedio, qint64(300));
}

void TstEstoqueRepository::entradaSemCustoMantemCusto()
{
    auto r = estoque();
    // Entrada sem custo (-1): soma quantidade, mantém custo médio.
    QVERIFY2(r.registrarEntrada(m_produtoId, 50, -1, 0, QString()),
             qUtf8Printable(r.ultimoErro()));
    const ItemEstoque it = r.item(m_produtoId);
    QCOMPARE(it.quantidade, qint64(250));
    QCOMPARE(it.custoMedio, qint64(300));
}

void TstEstoqueRepository::inventarioDefineQuantidade()
{
    auto r = estoque();
    QVERIFY2(r.registrarInventario(m_produtoId, 240, QStringLiteral("contagem"), 0),
             qUtf8Printable(r.ultimoErro()));
    const ItemEstoque it = r.item(m_produtoId);
    QCOMPARE(it.quantidade, qint64(240));
    QCOMPARE(it.custoMedio, qint64(300)); // inventário não mexe no custo
}

void TstEstoqueRepository::movimentacoesRegistradas()
{
    // 3 entradas + 1 inventário = 4 movimentações.
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM movimentacoes_estoque WHERE produto_id = :pid"));
    q.bindValue(QStringLiteral(":pid"), m_produtoId);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 4);

    // A última é o inventário com delta 240 - 250 = -10.
    QSqlQuery q2(m_db.connection());
    q2.prepare(QStringLiteral(
        "SELECT quantidade FROM movimentacoes_estoque "
        "WHERE produto_id = :pid AND tipo = 'inventario' ORDER BY id DESC LIMIT 1"));
    q2.bindValue(QStringLiteral(":pid"), m_produtoId);
    QVERIFY(q2.exec());
    QVERIFY(q2.next());
    QCOMPARE(q2.value(0).toLongLong(), qint64(-10));
}

void TstEstoqueRepository::retiradaBaixaEstoque()
{
    auto r = estoque();
    // Estoque atual = 240 (do inventário). Retira 40 (quebra) -> 200.
    QVERIFY2(r.registrarSaida(m_produtoId, 40, QStringLiteral("quebra"), 0),
             qUtf8Printable(r.ultimoErro()));
    QCOMPARE(r.item(m_produtoId).quantidade, qint64(200));

    // Pedir mais do que há falha (não deixa negativar por retirada manual).
    QVERIFY(!r.registrarSaida(m_produtoId, 999999, QStringLiteral("erro"), 0));
    QCOMPARE(r.item(m_produtoId).quantidade, qint64(200)); // inalterado
}

int TstEstoqueRepository::criarProduto(const QString &nome,
                                       const QVector<QPair<int, qint64>> &fatorPreco,
                                       const QString &unidadeBase)
{
    ProdutoRepository prepo(m_db.connection());
    Produto p;
    p.nome = nome;
    p.unidadeBase = unidadeBase;
    int n = 0;
    for (const auto &fp : fatorPreco) {
        Embalagem e;
        e.nome = QStringLiteral("Emb%1").arg(++n);
        e.fator = fp.first;
        e.precoVenda = fp.second;
        p.embalagens.push_back(e);
    }
    if (!prepo.salvar(p)) {
        qWarning() << prepo.ultimoErro();
        return 0;
    }
    return p.id;
}

// O número que o dono pediu na tela de Estoque: margem sobre o PREÇO DE VENDA,
// não markup. Custo R$ 10,00, venda R$ 15,00 -> lucro R$ 5,00 e margem 33,3%
// (o markup, que é sobre o custo, daria 50% — e é o engano que se quer evitar).
void TstEstoqueRepository::margemSobreOPrecoDeVenda()
{
    const int id = criarProduto(QStringLiteral("Margem Simples"), {{1, 1500}});
    QVERIFY(id > 0);
    auto r = estoque();
    QVERIFY2(r.registrarEntrada(id, 10, 1000, 0, QStringLiteral("compra")),
             qUtf8Printable(r.ultimoErro()));

    const ItemEstoque it = r.item(id);
    QCOMPARE(it.custoMedio, qint64(1000));
    QCOMPARE(it.precoBaseMilli, qint64(1500 * 1000));
    QVERIFY(it.margemDecimos().has_value());
    QCOMPARE(*it.margemDecimos(), 333);   // 33,3%

    // A listagem (o que a tela consome) enxerga o mesmo.
    bool achou = false;
    for (const ItemEstoque &linha : r.listar(QStringLiteral("Margem Simples"))) {
        if (linha.produtoId != id)
            continue;
        achou = true;
        QVERIFY(linha.margemDecimos().has_value());
        QCOMPARE(*linha.margemDecimos(), 333);
    }
    QVERIFY2(achou, "o produto nao apareceu na listagem de estoque");
}

// Preço de referência = a MENOR embalagem com preço (a unidade), levado para a
// unidade base — a mesma regra do aviso de custo. Pela caixa (R$ 4,00/un) a
// margem daria 25%, e as duas telas diriam coisas diferentes do mesmo produto.
void TstEstoqueRepository::margemUsaAMenorEmbalagemComPreco()
{
    const int id = criarProduto(QStringLiteral("Margem Caixa"), {{1, 450}, {12, 4800}});
    QVERIFY(id > 0);
    auto r = estoque();
    QVERIFY2(r.registrarEntrada(id, 12, 300, 0, QStringLiteral("compra")),
             qUtf8Printable(r.ultimoErro()));

    const ItemEstoque it = r.item(id);
    QCOMPARE(it.precoBaseMilli, qint64(450 * 1000));
    QCOMPARE(*it.margemDecimos(), 333);   // (450-300)/450, e não (400-300)/400
}

void TstEstoqueRepository::margemSemPrecoOuSemCustoNaoExiste()
{
    auto r = estoque();

    // Sem preço de venda não há sobre o que calcular margem.
    const int semPreco = criarProduto(QStringLiteral("Margem Sem Preco"), {{1, 0}});
    QVERIFY(semPreco > 0);
    QVERIFY2(r.registrarEntrada(semPreco, 5, 1000, 0, QStringLiteral("compra")),
             qUtf8Printable(r.ultimoErro()));
    QCOMPARE(r.item(semPreco).precoBaseMilli, qint64(0));
    QVERIFY(!r.item(semPreco).margemDecimos().has_value());

    // Custo 0 aqui significa DESCONHECIDO (bonificação, produto que nunca
    // entrou). Dizer "margem de 100%" seria mentira com cara de número certo.
    const int semCusto = criarProduto(QStringLiteral("Margem Sem Custo"), {{1, 1500}});
    QVERIFY(semCusto > 0);
    QCOMPARE(r.item(semCusto).custoMedioMilli, qint64(0));
    QVERIFY(!r.item(semCusto).margemDecimos().has_value());
}

// Prejuízo é exatamente o que se quer enxergar: sai negativo, não some.
void TstEstoqueRepository::margemNegativaQuandoOCustoPassaOPreco()
{
    const int id = criarProduto(QStringLiteral("Margem Prejuizo"), {{1, 450}});
    QVERIFY(id > 0);
    auto r = estoque();
    QVERIFY2(r.registrarEntrada(id, 3, 600, 0, QStringLiteral("compra cara")),
             qUtf8Printable(r.ultimoErro()));
    QCOMPARE(*r.item(id).margemDecimos(), -333);   // (450-600)/450
}

// Em ml o custo é fração de centavo. Se a margem saísse do custo arredondado
// (1 centavo/ml), a garrafa de R$ 18,99 mostraria 47,3% em vez de 42,1%.
void TstEstoqueRepository::margemEmMlNaoPerdeAFracaoDeCentavo()
{
    const int id = criarProduto(QStringLiteral("Margem Ml"), {{1000, 1899}},
                                QStringLiteral("ml"));
    QVERIFY(id > 0);
    auto r = estoque();
    // 1 garrafa de 1000 ml por R$ 11,00 => 1,1 centavo por ml (1100 milésimos).
    QVERIFY2(r.registrarEntradaMilli(id, 1000, 1100, 0, QStringLiteral("compra")),
             qUtf8Printable(r.ultimoErro()));

    const ItemEstoque it = r.item(id);
    QCOMPARE(it.custoMedioMilli, qint64(1100));
    QCOMPARE(it.custoMedio, qint64(1));            // arredondado, só para exibir
    QCOMPARE(it.precoBaseMilli, qint64(1899));     // 1,899 centavo por ml
    QCOMPARE(*it.margemDecimos(), 421);            // (1899-1100)/1899 = 42,1%
}

QTEST_MAIN(TstEstoqueRepository)
#include "tst_estoque_repository.moc"
