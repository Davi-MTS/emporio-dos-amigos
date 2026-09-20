#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"

// Verificação sobre uma CÓPIA do banco de verdade da loja.
//
// Ler o código não basta: os defeitos que apareceram na loja (fator errado,
// custo por ml, fiado sem vencimento) só ficaram visíveis olhando os dados. Este
// teste sobe as migrations na cópia e confere as invariantes que precisam valer
// sempre. Sem o caminho, é pulado — o banco da loja NÃO entra no repositório.
//
// Como rodar:
//   DISTRIBUIDORA_BANCO_REAL=/caminho/para/copia.db ./tst_banco_real.exe
class TstBancoReal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void migrationsAplicamEBancoFicaIntegro();
    void estoqueBateComAsMovimentacoes();
    void pagamentosBatemComOTotalDasVendas();
    void cadastroSemFatorAmbiguo();

private:
    QTemporaryDir m_dir;
    Database m_db;
    bool m_temBanco = false;

    qint64 escalar(const QString &sql)
    {
        QSqlQuery q(m_db.connection());
        return (q.exec(sql) && q.next()) ? q.value(0).toLongLong() : -1;
    }
};

void TstBancoReal::initTestCase()
{
    const QString origem = qEnvironmentVariable("DISTRIBUIDORA_BANCO_REAL");
    if (origem.isEmpty() || !QFileInfo::exists(origem))
        QSKIP("Defina DISTRIBUIDORA_BANCO_REAL com o caminho de uma cópia do banco da loja.");

    // Trabalha sempre numa cópia própria: o arquivo indicado não é tocado.
    const QString copia = m_dir.filePath(QStringLiteral("copia.db"));
    QVERIFY2(QFile::copy(origem, copia), "não consegui copiar o banco indicado");
    QVERIFY2(m_db.open(copia), qUtf8Printable(m_db.lastError()));
    m_temBanco = true;
}

void TstBancoReal::migrationsAplicamEBancoFicaIntegro()
{
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    qInfo("Migrations aplicadas nesta cópia: %s",
          qUtf8Printable(runner.appliedInLastRun().join(QStringLiteral(", "))));

    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("PRAGMA integrity_check")) && q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("ok"));
    QCOMPARE(escalar(QStringLiteral("PRAGMA foreign_key_check")), Q_INT64_C(-1));   // sem linhas = sem violação

    // A 0018 tirou as datas de cadastro do UTC: nada pode ter ficado no futuro.
    QCOMPARE(escalar(QStringLiteral(
                 "SELECT COUNT(*) FROM produtos WHERE criado_em > datetime('now','localtime','+1 hour')")),
             Q_INT64_C(0));
}

// O saldo de cada produto tem que ser exatamente a soma das movimentações.
void TstBancoReal::estoqueBateComAsMovimentacoes()
{
    const qint64 divergentes = escalar(QStringLiteral(
        "SELECT COUNT(*) FROM estoque e WHERE e.quantidade_atual <> "
        "  COALESCE((SELECT SUM(m.quantidade) FROM movimentacoes_estoque m "
        "            WHERE m.produto_id = e.produto_id), 0)"));
    QCOMPARE(divergentes, Q_INT64_C(0));
}

// Toda venda concluída precisa ter pagamento suficiente, e o troco nunca pode
// passar do dinheiro recebido (foi o que gerou caixa esperado negativo).
void TstBancoReal::pagamentosBatemComOTotalDasVendas()
{
    QCOMPARE(escalar(QStringLiteral(
                 "SELECT COUNT(*) FROM vendas v WHERE v.status = 'concluida' AND "
                 "  COALESCE((SELECT SUM(p.valor) FROM pagamentos p WHERE p.venda_id = v.id), 0) "
                 "  < v.total")),
             Q_INT64_C(0));
    QCOMPARE(escalar(QStringLiteral(
                 "SELECT COUNT(*) FROM vendas v WHERE v.troco > "
                 "  COALESCE((SELECT SUM(p.valor) FROM pagamentos p "
                 "            WHERE p.venda_id = v.id AND p.forma = 'dinheiro'), 0)")),
             Q_INT64_C(0));
}

// Duas embalagens do mesmo produto com o mesmo fator e preços diferentes é o
// erro de cadastro que fez caixinha baixar 1 lata. O sistema agora recusa isso
// no salvar; aqui só listamos o que já está gravado, para o dono corrigir.
void TstBancoReal::cadastroSemFatorAmbiguo()
{
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "SELECT p.nome, a.nome_embalagem, b.nome_embalagem, a.fator_conversao "
        "FROM produto_embalagens a "
        "JOIN produto_embalagens b ON b.produto_id = a.produto_id AND b.id > a.id "
        "   AND b.fator_conversao = a.fator_conversao AND b.preco_venda <> a.preco_venda "
        "JOIN produtos p ON p.id = a.produto_id WHERE p.ativo = 1 ORDER BY p.nome")));
    int n = 0;
    while (q.next()) {
        ++n;
        qWarning("CADASTRO A CORRIGIR: %s — \"%s\" e \"%s\" com o mesmo fator (%d) e preços diferentes",
                 qUtf8Printable(q.value(0).toString()), qUtf8Printable(q.value(1).toString()),
                 qUtf8Printable(q.value(2).toString()), q.value(3).toInt());
    }
    qInfo("Produtos com fator ambíguo neste banco: %d", n);
}

QTEST_MAIN(TstBancoReal)
#include "tst_banco_real.moc"
