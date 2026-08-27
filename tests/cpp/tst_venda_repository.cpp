#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/caixa/CaixaRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"
#include "domain/vendas/VendaRepository.h"

class TstVendaRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void vendaBaixaEstoqueECalculaTroco();
    void pagamentoInsuficienteFalha();
    void excedenteEmPixNaoViraTroco();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0;
    int m_sessaoId = 0;
    int m_produtoId = 0;
    int m_embBaseId = 0;
    int m_embCaixaId = 0;
};

void TstVendaRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    // Usuário (FK de sessão/venda).
    QSqlQuery u(m_db.connection());
    QVERIFY(u.exec(QStringLiteral(
        "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) "
        "VALUES (1, 'Admin', 'admin', '', 1)")));
    m_usuarioId = u.lastInsertId().toInt();
    QVERIFY(m_usuarioId > 0);

    // Produto com embalagem base (fator 1, 5,00) e caixa (fator 12, 60,00).
    ProdutoRepository prepo(m_db.connection());
    Produto p;
    p.nome = QStringLiteral("Produto Venda");
    Embalagem base; base.nome = QStringLiteral("Unidade"); base.fator = 1; base.precoVenda = 500;
    Embalagem caixa; caixa.nome = QStringLiteral("Caixa"); caixa.fator = 12; caixa.precoVenda = 6000;
    p.embalagens = {base, caixa};
    QVERIFY2(prepo.salvar(p), qUtf8Printable(prepo.ultimoErro()));
    m_produtoId = p.id;
    for (const Embalagem &e : p.embalagens) {
        if (e.fator == 1) m_embBaseId = e.id;
        else if (e.fator == 12) m_embCaixaId = e.id;
    }
    QVERIFY(m_embBaseId > 0 && m_embCaixaId > 0);

    // Estoque inicial: 100 unidades base.
    EstoqueRepository erepo(m_db.connection());
    QVERIFY2(erepo.registrarEntrada(m_produtoId, 100, 380, m_usuarioId, QString()),
             qUtf8Printable(erepo.ultimoErro()));

    // Abre caixa.
    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(10000, m_usuarioId);
    QVERIFY(m_sessaoId > 0);
}

void TstVendaRepository::vendaBaixaEstoqueECalculaTroco()
{
    VendaRepository vrepo(m_db.connection());

    QVector<LinhaVenda> itens;
    LinhaVenda l1; l1.produtoId = m_produtoId; l1.embalagemId = m_embBaseId; l1.fator = 1;
    l1.qtdEmbalagem = 2; l1.precoUnit = 500; itens.push_back(l1);       // 2 un x 5,00 = 10,00
    LinhaVenda l2; l2.produtoId = m_produtoId; l2.embalagemId = m_embCaixaId; l2.fator = 12;
    l2.qtdEmbalagem = 1; l2.precoUnit = 6000; itens.push_back(l2);      // 1 caixa = 60,00

    QVector<PagamentoVenda> pags;
    PagamentoVenda pg; pg.forma = QStringLiteral("dinheiro"); pg.valor = 10000; // paga 100,00
    pags.push_back(pg);

    const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
    QVERIFY2(r.ok, qUtf8Printable(r.erro));
    QCOMPARE(r.total, qint64(7000));   // 10,00 + 60,00 = 70,00
    QCOMPARE(r.troco, qint64(3000));   // 100 - 70 = 30,00

    // Estoque: 100 - (2 + 12) = 86.
    EstoqueRepository erepo(m_db.connection());
    QCOMPARE(erepo.item(m_produtoId).quantidade, qint64(86));

    // 2 itens, 1 pagamento, 2 movimentações de saída.
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM venda_itens WHERE venda_id = %1").arg(r.vendaId)));
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 2);

    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM pagamentos WHERE venda_id = %1").arg(r.vendaId)));
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 1);

    QVERIFY(q.exec(QStringLiteral(
        "SELECT COUNT(*) FROM movimentacoes_estoque "
        "WHERE produto_id = %1 AND tipo = 'saida_venda'").arg(m_produtoId)));
    QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 2);
}

void TstVendaRepository::pagamentoInsuficienteFalha()
{
    VendaRepository vrepo(m_db.connection());
    QVector<LinhaVenda> itens;
    LinhaVenda l; l.produtoId = m_produtoId; l.embalagemId = m_embBaseId; l.fator = 1;
    l.qtdEmbalagem = 1; l.precoUnit = 500; itens.push_back(l);   // 5,00
    QVector<PagamentoVenda> pags;
    PagamentoVenda pg; pg.forma = QStringLiteral("dinheiro"); pg.valor = 300; // só 3,00
    pags.push_back(pg);

    const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
    QVERIFY(!r.ok);
    QVERIFY(!r.erro.isEmpty());

    // Estoque não mudou (continua 86).
    EstoqueRepository erepo(m_db.connection());
    QCOMPARE(erepo.item(m_produtoId).quantidade, qint64(86));
}

void TstVendaRepository::excedenteEmPixNaoViraTroco()
{
    VendaRepository vrepo(m_db.connection());
    QVector<LinhaVenda> itens;
    LinhaVenda l; l.produtoId = m_produtoId; l.embalagemId = m_embBaseId; l.fator = 1;
    l.qtdEmbalagem = 1; l.precoUnit = 500; itens.push_back(l);   // venda de 5,00

    // Pix lançado em dobro (erro de operação): 10,00 para uma venda de 5,00.
    // NÃO pode gerar troco — não entrou dinheiro na gaveta para devolver.
    {
        QVector<PagamentoVenda> pags;
        PagamentoVenda p1; p1.forma = QStringLiteral("pix"); p1.valor = 500; pags.push_back(p1);
        PagamentoVenda p2; p2.forma = QStringLiteral("pix"); p2.valor = 500; pags.push_back(p2);
        const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
        QVERIFY2(r.ok, qUtf8Printable(r.erro));
        QCOMPARE(r.total, qint64(500));
        QCOMPARE(r.troco, qint64(0));   // antes vinha 500 e zerava a gaveta
    }

    // Misto: 3,00 em pix + 5,00 em dinheiro numa venda de 5,00 -> troco 3,00
    // (limitado ao dinheiro entregue, nunca ao excedente do pix).
    {
        QVector<PagamentoVenda> pags;
        PagamentoVenda p1; p1.forma = QStringLiteral("pix"); p1.valor = 300; pags.push_back(p1);
        PagamentoVenda p2; p2.forma = QStringLiteral("dinheiro"); p2.valor = 500; pags.push_back(p2);
        const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
        QVERIFY2(r.ok, qUtf8Printable(r.erro));
        QCOMPARE(r.troco, qint64(300));
    }
}

QTEST_MAIN(TstVendaRepository)
#include "tst_venda_repository.moc"
