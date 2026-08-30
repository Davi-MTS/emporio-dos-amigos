#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/financeiro/FinanceiroRepository.h"

// Desfazer o pagamento de uma conta mexe em DOIS lugares: a conta volta a ficar
// aberta e, se o dinheiro tinha saído da gaveta, ele precisa voltar para a
// gaveta. Errar metade disso é pior que não ter o recurso: a conta reabre e o
// dinheiro some da conferência do caixa (ou aparece duas vezes).
//
// Vai pelo AppBackend de propósito — é lá que a conta e o caixa são amarrados
// na mesma transação.
class TstEstorno : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pagamentoEmDinheiroSaiDaGaveta();
    void estornoDevolveODinheiroParaAGaveta();
    void estornoDePixNaoTocaNoCaixa();
    void naoEstornaContaQueNaoFoiPaga();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;

    int criarDespesa(const QString &nome, qint64 valor);
    FinanceiroRepository fin() { return FinanceiroRepository(m_db.connection()); }
};

void TstEstorno::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    // Caixa aberto com R$ 200,00 de troco.
    QVERIFY2(m_app->abrirCaixa(QStringLiteral("200,00")), qUtf8Printable(m_app->ultimoErro()));
    QVERIFY(m_app->caixaAberto());
}

int TstEstorno::criarDespesa(const QString &nome, qint64 valor)
{
    auto f = fin();
    if (!f.criarDespesa(nome, valor, QString()))
        return 0;
    const auto abertas = f.contasPagar(true);
    for (const auto &c : abertas) {
        if (c.descricao == nome)
            return c.id;
    }
    return 0;
}

// Pagar em dinheiro tira da gaveta: vira sangria e o esperado cai.
void TstEstorno::pagamentoEmDinheiroSaiDaGaveta()
{
    const int id = criarDespesa(QStringLiteral("Gás do fogão"), 5000);
    QVERIFY(id > 0);

    const QVariantMap antes = m_app->caixaResumo();
    const qint64 esperadoAntes = antes.value(QStringLiteral("dinheiroEsperado")).toLongLong();

    // A tela avisa o operador ANTES de confirmar: confere o que ela vai dizer.
    const QVariantMap efeito = m_app->efeitoDoPagamento(QStringLiteral("dinheiro"), 5000);
    QCOMPARE(efeito.value(QStringLiteral("caixaAberto")).toBool(), true);
    QCOMPARE(efeito.value(QStringLiteral("gavetaAgora")).toLongLong(), esperadoAntes);
    QCOMPARE(efeito.value(QStringLiteral("gavetaDepois")).toLongLong(), esperadoAntes - 5000);

    QVERIFY2(m_app->pagarConta(id, QStringLiteral("dinheiro")),
             qUtf8Printable(m_app->ultimoErro()));

    const QVariantMap depois = m_app->caixaResumo();
    QCOMPARE(depois.value(QStringLiteral("dinheiroEsperado")).toLongLong(), esperadoAntes - 5000);
    QCOMPARE(depois.value(QStringLiteral("sangrias")).toLongLong(),
             antes.value(QStringLiteral("sangrias")).toLongLong() + 5000);

    // E a forma ficou gravada — é ela que permite estornar sem chutar.
    const auto pagas = fin().contasPagar(false);
    bool achou = false;
    for (const auto &c : pagas) {
        if (c.id == id) {
            QCOMPARE(c.status, QStringLiteral("paga"));
            QCOMPARE(c.formaPagamento, QStringLiteral("dinheiro"));
            achou = true;
        }
    }
    QVERIFY(achou);
}

// Desfazer devolve o dinheiro para a gaveta e reabre a conta.
void TstEstorno::estornoDevolveODinheiroParaAGaveta()
{
    const int id = criarDespesa(QStringLiteral("Compra errada"), 3000);
    QVERIFY(id > 0);

    const qint64 esperadoAntes =
        m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();
    QVERIFY(m_app->pagarConta(id, QStringLiteral("dinheiro")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(),
             esperadoAntes - 3000);

    const QVariantMap r = m_app->estornarPagamento(id);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    // A gaveta voltou ao que era.
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(),
             esperadoAntes);
    // E entrou como suprimento, com rastro — não some do fechamento.
    QVERIFY(!r.value(QStringLiteral("aviso")).toString().isEmpty());

    // A conta voltou para a lista de abertas, sem forma de pagamento.
    bool aberta = false;
    for (const auto &c : fin().contasPagar(true)) {
        if (c.id == id) {
            aberta = true;
            QVERIFY(c.formaPagamento.isEmpty());
            QVERIFY(c.pagoEm.isEmpty());
        }
    }
    QVERIFY2(aberta, "a conta estornada tem que voltar para as abertas");
}

// Pix não passa pela gaveta: estornar não pode inventar dinheiro no caixa.
void TstEstorno::estornoDePixNaoTocaNoCaixa()
{
    const int id = criarDespesa(QStringLiteral("Internet"), 12000);
    QVERIFY(id > 0);

    const qint64 esperado =
        m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();

    const QVariantMap efeito = m_app->efeitoDoPagamento(QStringLiteral("pix"), 12000);
    QVERIFY(!efeito.contains(QStringLiteral("gavetaDepois")));

    QVERIFY(m_app->pagarConta(id, QStringLiteral("pix")));
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(),
             esperado);

    const QVariantMap r = m_app->estornarPagamento(id);
    QVERIFY(r.value(QStringLiteral("ok")).toBool());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(),
             esperado);
}

// Conta que nunca foi paga não pode "voltar" — e não pode gerar dinheiro.
void TstEstorno::naoEstornaContaQueNaoFoiPaga()
{
    const int id = criarDespesa(QStringLiteral("Aluguel"), 90000);
    QVERIFY(id > 0);

    const qint64 esperado =
        m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong();

    const QVariantMap r = m_app->estornarPagamento(id);
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(!r.value(QStringLiteral("erro")).toString().isEmpty());
    QCOMPARE(m_app->caixaResumo().value(QStringLiteral("dinheiroEsperado")).toLongLong(),
             esperado);
}

QTEST_MAIN(TstEstorno)
#include "tst_estorno.moc"
