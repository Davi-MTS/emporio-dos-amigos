#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/caixa/CaixaRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"
#include "domain/relatorios/RelatorioRepository.h"
#include "domain/vendas/VendaRepository.h"

class TstRelatorioRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dashboardConfere();
    void faturamentoELucro();
    void vendasPorFormaEMaisVendidos();
    void produtosParados();
    void lucroTravadoNoMomentoDaVenda();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0, m_sessaoId = 0;
    int m_prodA = 0, m_embA = 0, m_prodB = 0;
    RelatorioRepository rel() { return RelatorioRepository(m_db.connection()); }
};

void TstRelatorioRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery u(m_db.connection());
    QVERIFY(u.exec(QStringLiteral(
        "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) VALUES (1,'A','a','x',1)")));
    m_usuarioId = u.lastInsertId().toInt();

    ProdutoRepository prepo(m_db.connection());
    Produto a; a.nome = QStringLiteral("Produto A"); a.estoqueMinimo = 0;
    Embalagem ea; ea.nome = QStringLiteral("Unidade"); ea.fator = 1; ea.precoVenda = 500;
    a.embalagens = {ea};
    QVERIFY(prepo.salvar(a)); m_prodA = a.id; m_embA = a.embalagens.first().id;

    Produto b; b.nome = QStringLiteral("Produto B"); b.estoqueMinimo = 1000; // ficará em falta
    Embalagem eb; eb.nome = QStringLiteral("Unidade"); eb.fator = 1; eb.precoVenda = 900;
    b.embalagens = {eb};
    QVERIFY(prepo.salvar(b)); m_prodB = b.id;

    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_prodA, 100, 300, m_usuarioId, QString())); // custo 3,00
    QVERIFY(erepo.registrarEntrada(m_prodB, 10, 500, m_usuarioId, QString()));

    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(0, m_usuarioId);

    VendaRepository vrepo(m_db.connection());
    // Venda 1: 3 un de A (15,00) em dinheiro.
    {
        QVector<LinhaVenda> it; LinhaVenda l; l.produtoId=m_prodA; l.embalagemId=m_embA; l.fator=1; l.qtdEmbalagem=3; l.precoUnit=500; it.push_back(l);
        QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma="dinheiro"; p.valor=1500; pg.push_back(p);
        QVERIFY(vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId).ok);
    }
    // Venda 2: 2 un de A (10,00) em dinheiro.
    {
        QVector<LinhaVenda> it; LinhaVenda l; l.produtoId=m_prodA; l.embalagemId=m_embA; l.fator=1; l.qtdEmbalagem=2; l.precoUnit=500; it.push_back(l);
        QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma="dinheiro"; p.valor=1000; pg.push_back(p);
        QVERIFY(vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId).ok);
    }
}

void TstRelatorioRepository::dashboardConfere()
{
    const DashboardKpis k = rel().dashboard();
    QCOMPARE(k.vendasHoje, qint64(2500));
    QCOMPARE(k.numVendasHoje, 2);
    QCOMPARE(k.ticketMedio, qint64(1250));
    QCOMPARE(k.produtosEmFalta, 1);   // Produto B
}

void TstRelatorioRepository::faturamentoELucro()
{
    const FaturamentoResumo f = rel().faturamento(0); // hoje
    QCOMPARE(f.total, qint64(2500));
    QCOMPARE(f.numVendas, 2);
    // Custo: 5 un de A * 3,00 = 15,00. Lucro = 25,00 - 15,00 = 10,00.
    QCOMPARE(f.custo, qint64(1500));
    QCOMPARE(f.lucro, qint64(1000));
}

void TstRelatorioRepository::vendasPorFormaEMaisVendidos()
{
    const auto formas = rel().vendasPorForma(0);
    QCOMPARE(formas.size(), 1);
    QCOMPARE(formas.first().forma, QStringLiteral("dinheiro"));
    QCOMPARE(formas.first().total, qint64(2500));

    const auto top = rel().maisVendidos(0, 5);
    QVERIFY(!top.isEmpty());
    QCOMPARE(top.first().nome, QStringLiteral("Produto A"));
    QCOMPARE(top.first().qtd, qint64(5));
}

void TstRelatorioRepository::produtosParados()
{
    const auto parados = rel().produtosParados(0);
    // Produto B não vendeu hoje.
    bool achouB = false;
    for (const ProdutoParado &p : parados)
        if (p.nome == QStringLiteral("Produto B")) achouB = true;
    QVERIFY(achouB);
    // Produto A vendeu, não deve estar parado.
    for (const ProdutoParado &p : parados)
        QVERIFY(p.nome != QStringLiteral("Produto A"));
}

void TstRelatorioRepository::lucroTravadoNoMomentoDaVenda()
{
    // As 5 unidades de A foram vendidas com custo médio 3,00 (agora travado na
    // movimentação). Uma nova entrada bem mais cara eleva o custo médio atual.
    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_prodA, 100, 900, m_usuarioId, QString())); // 9,00/un

    // O lucro das vendas de hoje NÃO muda: continua valendo 3,00/un (custo do
    // momento), e não o novo custo médio.
    const FaturamentoResumo f = rel().faturamento(0);
    QCOMPARE(f.total, qint64(2500));
    QCOMPARE(f.custo, qint64(1500));   // 5 * 3,00 travado
    QCOMPARE(f.lucro, qint64(1000));
}

QTEST_MAIN(TstRelatorioRepository)
#include "tst_relatorio_repository.moc"
