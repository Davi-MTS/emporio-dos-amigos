#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/caixa/CaixaRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"
#include "domain/vendas/VendaRepository.h"

class TstCaixaRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void resumoConfereEsperado();
    void fechamentoRegistraDiferenca();
    void naoFechaDuasVezes();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0;
    int m_sessaoId = 0;
    int m_produtoId = 0;
    int m_embBaseId = 0;
    int m_embCaixaId = 0;
};

void TstCaixaRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery u(m_db.connection());
    QVERIFY(u.exec(QStringLiteral(
        "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) "
        "VALUES (1, 'Admin', 'admin', '', 1)")));
    m_usuarioId = u.lastInsertId().toInt();

    ProdutoRepository prepo(m_db.connection());
    Produto p;
    p.nome = QStringLiteral("Produto Caixa");
    Embalagem base; base.nome = QStringLiteral("Unidade"); base.fator = 1; base.precoVenda = 500;
    Embalagem cx; cx.nome = QStringLiteral("Caixa"); cx.fator = 12; cx.precoVenda = 6000;
    p.embalagens = {base, cx};
    QVERIFY2(prepo.salvar(p), qUtf8Printable(prepo.ultimoErro()));
    m_produtoId = p.id;
    for (const Embalagem &e : p.embalagens) {
        if (e.fator == 1) m_embBaseId = e.id; else m_embCaixaId = e.id;
    }

    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_produtoId, 100, 380, m_usuarioId, QString()));

    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(10000, m_usuarioId); // abertura 100,00
    QVERIFY(m_sessaoId > 0);

    VendaRepository vrepo(m_db.connection());
    // Venda 1: 1 unidade (5,00), paga 10,00 em dinheiro -> troco 5,00.
    {
        QVector<LinhaVenda> itens;
        LinhaVenda l; l.produtoId = m_produtoId; l.embalagemId = m_embBaseId; l.fator = 1;
        l.qtdEmbalagem = 1; l.precoUnit = 500; itens.push_back(l);
        QVector<PagamentoVenda> pags;
        PagamentoVenda pg; pg.forma = QStringLiteral("dinheiro"); pg.valor = 1000; pags.push_back(pg);
        const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
        QVERIFY2(r.ok, qUtf8Printable(r.erro));
        QCOMPARE(r.troco, qint64(500));
    }
    // Venda 2: 1 caixa (60,00), paga 60,00 em pix -> sem troco.
    {
        QVector<LinhaVenda> itens;
        LinhaVenda l; l.produtoId = m_produtoId; l.embalagemId = m_embCaixaId; l.fator = 12;
        l.qtdEmbalagem = 1; l.precoUnit = 6000; itens.push_back(l);
        QVector<PagamentoVenda> pags;
        PagamentoVenda pg; pg.forma = QStringLiteral("pix"); pg.valor = 6000; pags.push_back(pg);
        const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
        QVERIFY2(r.ok, qUtf8Printable(r.erro));
    }

    // Suprimento 20,00 e sangria 5,00.
    QVERIFY(crepo.registrarMovimento(m_sessaoId, QStringLiteral("suprimento"), 2000, QString(), m_usuarioId));
    QVERIFY(crepo.registrarMovimento(m_sessaoId, QStringLiteral("sangria"), 500, QString(), m_usuarioId));
    // Recebimento de fiado em dinheiro 30,00 (entra na gaveta).
    QVERIFY(crepo.registrarRecebimento(m_sessaoId, 3000, QStringLiteral("Recebimento de fiado"), m_usuarioId));
}

void TstCaixaRepository::resumoConfereEsperado()
{
    CaixaRepository crepo(m_db.connection());
    const ResumoCaixa r = crepo.resumo(m_sessaoId);

    QCOMPARE(r.abertura, qint64(10000));
    QCOMPARE(r.vendasDinheiro, qint64(1000));
    QCOMPARE(r.vendasPix, qint64(6000));
    QCOMPARE(r.troco, qint64(500));
    QCOMPARE(r.suprimentos, qint64(2000));
    QCOMPARE(r.sangrias, qint64(500));
    QCOMPARE(r.recebimentos, qint64(3000));
    QCOMPARE(r.numVendas, 2);
    // Vendido = 5,00 + 60,00 = 65,00 (NÃO 70,00: o cliente entregou 10,00 na
    // primeira venda e levou 5,00 de troco — isso não é venda).
    QCOMPARE(r.totalVendas(), qint64(6500));
    QCOMPARE(r.totalRecebidoPorForma(), qint64(7000));
    // 10000 + 1000 - 500 + 2000 - 500 + 3000 = 15000
    QCOMPARE(r.dinheiroEsperado(), qint64(15000));
}

void TstCaixaRepository::fechamentoRegistraDiferenca()
{
    CaixaRepository crepo(m_db.connection());
    // Operador contou 148,00 (esperado 150,00) -> falta 2,00.
    const ResultadoFechamento f = crepo.fechar(m_sessaoId, 14800, m_usuarioId);
    QVERIFY2(f.ok, qUtf8Printable(f.erro));
    QCOMPARE(f.esperado, qint64(15000));
    QCOMPARE(f.informado, qint64(14800));
    QCOMPARE(f.diferenca, qint64(-200));

    // Persistiu e a sessão está fechada.
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT status, diferenca FROM sessoes_caixa WHERE id = %1").arg(m_sessaoId)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("fechada"));
    QCOMPARE(q.value(1).toLongLong(), qint64(-200));

    QCOMPARE(crepo.sessaoAbertaId(), 0);
}

void TstCaixaRepository::naoFechaDuasVezes()
{
    CaixaRepository crepo(m_db.connection());
    const ResultadoFechamento f = crepo.fechar(m_sessaoId, 12000, m_usuarioId);
    QVERIFY(!f.ok);
    QVERIFY(!f.erro.isEmpty());
}

QTEST_MAIN(TstCaixaRepository)
#include "tst_caixa_repository.moc"
