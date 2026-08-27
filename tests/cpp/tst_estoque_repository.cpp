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

private:
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

QTEST_MAIN(TstEstoqueRepository)
#include "tst_estoque_repository.moc"
