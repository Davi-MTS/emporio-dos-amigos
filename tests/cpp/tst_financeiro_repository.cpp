#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/financeiro/FinanceiroRepository.h"

class TstFinanceiroRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void despesaEPagamento();
    void contaReceberEBaixa();
    void resumoConfere();
    void recebimentoParcialConta();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_clienteId = 0;
    FinanceiroRepository fin() { return FinanceiroRepository(m_db.connection()); }
};

void TstFinanceiroRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery c(m_db.connection());
    QVERIFY(c.exec(QStringLiteral(
        "INSERT INTO clientes (nome, limite_fiado, ativo) VALUES ('Cli', 10000, 1)")));
    m_clienteId = c.lastInsertId().toInt();
    // Conta a receber aberta de 40,00.
    QSqlQuery r(m_db.connection());
    r.prepare(QStringLiteral(
        "INSERT INTO contas_receber (cliente_id, valor, status) VALUES (:c, 4000, 'aberta')"));
    r.bindValue(QStringLiteral(":c"), m_clienteId);
    QVERIFY(r.exec());
}

void TstFinanceiroRepository::despesaEPagamento()
{
    auto f = fin();
    QVERIFY2(f.criarDespesa(QStringLiteral("Aluguel"), 15000, QStringLiteral("2026-09-05")),
             qUtf8Printable(f.ultimoErro()));

    auto abertas = f.contasPagar(true);
    QCOMPARE(abertas.size(), 1);
    QCOMPARE(abertas.first().valor, qint64(15000));
    const int id = abertas.first().id;

    QVERIFY(f.pagar(id));
    QCOMPARE(f.contasPagar(true).size(), 0);   // não há mais abertas
    QCOMPARE(f.contasPagar(false).size(), 1);  // continua no histórico (paga)
}

void TstFinanceiroRepository::contaReceberEBaixa()
{
    auto f = fin();
    auto abertas = f.contasReceber(true);
    QCOMPARE(abertas.size(), 1);
    QCOMPARE(abertas.first().valor, qint64(4000));
    QCOMPARE(abertas.first().clienteNome, QStringLiteral("Cli"));

    QVERIFY(f.receber(abertas.first().id));
    QCOMPARE(f.contasReceber(true).size(), 0);
}

void TstFinanceiroRepository::resumoConfere()
{
    auto f = fin();
    // Após pagar a despesa e receber a conta, ambos zerados.
    const ResumoFinanceiro r = f.resumo();
    QCOMPARE(r.totalAPagar, qint64(0));
    QCOMPARE(r.totalAReceber, qint64(0));

    // Nova despesa de 100,00 -> a pagar 10000, saldo previsto -10000.
    QVERIFY(f.criarDespesa(QStringLiteral("Energia"), 10000, QString()));
    const ResumoFinanceiro r2 = f.resumo();
    QCOMPARE(r2.totalAPagar, qint64(10000));
    QCOMPARE(r2.saldoPrevisto(), qint64(-10000));
}

void TstFinanceiroRepository::recebimentoParcialConta()
{
    auto f = fin();
    // Nova conta a receber de 60,00.
    QSqlQuery ins(m_db.connection());
    ins.prepare(QStringLiteral(
        "INSERT INTO contas_receber (cliente_id, valor, status) VALUES (:c, 6000, 'aberta')"));
    ins.bindValue(QStringLiteral(":c"), m_clienteId);
    QVERIFY(ins.exec());
    const int id = ins.lastInsertId().toInt();

    // Recebe 25,00 -> conta continua aberta com 35,00.
    QCOMPARE(f.aplicarRecebimentoConta(id, 2500), qint64(2500));
    // Recebe além do saldo -> aplica só os 35,00 que restam e fecha a conta.
    QCOMPARE(f.aplicarRecebimentoConta(id, 9999999), qint64(3500));
    // Nova tentativa não aplica nada (já paga).
    QCOMPARE(f.aplicarRecebimentoConta(id, 1000), qint64(0));

    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT status FROM contas_receber WHERE id = %1").arg(id)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("paga"));
}

QTEST_MAIN(TstFinanceiroRepository)
#include "tst_financeiro_repository.moc"
