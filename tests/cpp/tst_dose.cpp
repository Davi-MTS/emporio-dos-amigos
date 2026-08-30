#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"

// Dose = produto próprio ligado à garrafa. Vende com um bipe, mas o estoque que
// sai é o da GARRAFA, em ml. Se isso errar, a loja vende dose o dia inteiro com
// a garrafa marcando estoque cheio — e ninguém percebe até faltar bebida.
class TstDose : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void doseNaoTemEstoqueProprio();
    void disponivelVemDaGarrafa();
    void venderDoseBaixaAGarrafa();
    void doseNaoApareceNaListaDeEstoque();
    void doseSemOrigemVoltaASerProdutoNormal();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_garrafaId = 0;
    int m_doseId = 0;

    ProdutoRepository prod() { return ProdutoRepository(m_db.connection()); }
    EstoqueRepository estoque() { return EstoqueRepository(m_db.connection()); }
};

void TstDose::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    const int catId = m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();

    // Garrafa de 1 litro, contada em ml.
    {
        QVariantMap g = m_app->novoProduto();
        g[QStringLiteral("nome")] = QStringLiteral("Whisky Teste");
        g[QStringLiteral("categoriaId")] = catId;
        g[QStringLiteral("unidadeBase")] = QStringLiteral("ml");
        QVariantMap emb;
        emb[QStringLiteral("id")] = 0;
        emb[QStringLiteral("nome")] = QStringLiteral("Garrafa 1 L");
        emb[QStringLiteral("fator")] = 1000;      // 1 garrafa = 1000 ml
        emb[QStringLiteral("preco")] = 12000;
        emb[QStringLiteral("custo")] = -1;
        g[QStringLiteral("embalagens")] = QVariantList{emb};
        QVERIFY2(m_app->salvarProduto(g), qUtf8Printable(m_app->ultimoErro()));
    }
    for (const Produto &p : prod().listar(QStringLiteral("Whisky Teste")))
        m_garrafaId = p.id;
    QVERIFY(m_garrafaId > 0);

    // Dose de 50 ml que sai dessa garrafa.
    {
        QVariantMap d = m_app->novoProduto();
        d[QStringLiteral("nome")] = QStringLiteral("Dose de Whisky");
        d[QStringLiteral("categoriaId")] = catId;
        d[QStringLiteral("unidadeBase")] = QStringLiteral("unidade");
        d[QStringLiteral("doseDeProdutoId")] = m_garrafaId;
        d[QStringLiteral("doseQuantidade")] = 50;
        QVariantMap emb;
        emb[QStringLiteral("id")] = 0;
        emb[QStringLiteral("nome")] = QStringLiteral("Dose");
        emb[QStringLiteral("fator")] = 1;
        emb[QStringLiteral("codigoBarras")] = QStringLiteral("DOSE50");
        emb[QStringLiteral("preco")] = 1000;
        emb[QStringLiteral("custo")] = -1;
        d[QStringLiteral("embalagens")] = QVariantList{emb};
        QVERIFY2(m_app->salvarProduto(d), qUtf8Printable(m_app->ultimoErro()));
    }
    for (const Produto &p : prod().listar(QStringLiteral("Dose de Whisky")))
        m_doseId = p.id;
    QVERIFY(m_doseId > 0);

    // Duas garrafas na prateleira = 2000 ml.
    QVERIFY(m_app->abrirCaixa(QStringLiteral("100,00")));
    QVERIFY2(m_app->registrarEntrada(m_garrafaId, prod().obter(m_garrafaId)->embalagens.first().id,
                                     2, QStringLiteral("80,00"), QStringLiteral("carga")),
             qUtf8Printable(m_app->ultimoErro()));
    QCOMPARE(estoque().item(m_garrafaId).quantidade, qint64(2000));
}

// A dose não pode ter linha de estoque: seriam dois saldos para a mesma bebida.
void TstDose::doseNaoTemEstoqueProprio()
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM estoque WHERE produto_id = :id"));
    q.bindValue(QStringLiteral(":id"), m_doseId);
    QVERIFY(q.exec() && q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

// 2000 ml na garrafa, dose de 50 ml => 40 doses.
void TstDose::disponivelVemDaGarrafa()
{
    QCOMPARE(m_app->estoqueDisponivel(m_doseId), qlonglong(40));
    QCOMPARE(m_app->estoqueDisponivel(m_garrafaId), qlonglong(2000));
}

// Vender 3 doses tem que tirar 150 ml da garrafa — e nada mais.
void TstDose::venderDoseBaixaAGarrafa()
{
    const qint64 antes = estoque().item(m_garrafaId).quantidade;

    QVariantMap item;
    item[QStringLiteral("produtoId")] = m_doseId;
    item[QStringLiteral("embalagemId")] = prod().obter(m_doseId)->embalagens.first().id;
    item[QStringLiteral("fator")] = 1;
    item[QStringLiteral("qtd")] = 3;
    item[QStringLiteral("precoUnit")] = 1000;
    item[QStringLiteral("desconto")] = 0;

    QVariantMap pag;
    pag[QStringLiteral("forma")] = QStringLiteral("dinheiro");
    pag[QStringLiteral("valor")] = 3000;

    QVariantMap venda;
    venda[QStringLiteral("desconto")] = 0;
    venda[QStringLiteral("clienteId")] = 0;
    venda[QStringLiteral("itens")] = QVariantList{item};
    venda[QStringLiteral("pagamentos")] = QVariantList{pag};

    const QVariantMap r = m_app->finalizarVenda(venda);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    QCOMPARE(estoque().item(m_garrafaId).quantidade, antes - 150);
    // E a dose continua sem estoque próprio (não ficou negativa em lugar nenhum).
    QCOMPARE(m_app->estoqueDisponivel(m_doseId), qlonglong((antes - 150) / 50));

    // Cancelar devolve os 150 ml à garrafa.
    const int vendaId = r.value(QStringLiteral("vendaId")).toInt();
    const QVariantMap c = m_app->cancelarVenda(vendaId, QStringLiteral("teste"));
    QVERIFY2(c.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(c.value(QStringLiteral("erro")).toString()));
    QCOMPARE(estoque().item(m_garrafaId).quantidade, antes);
}

// Na tela de Estoque a dose não aparece: não há saldo dela para inventariar.
void TstDose::doseNaoApareceNaListaDeEstoque()
{
    bool achouGarrafa = false;
    bool achouDose = false;
    for (const auto &it : estoque().listar()) {
        if (it.produtoId == m_garrafaId) achouGarrafa = true;
        if (it.produtoId == m_doseId)    achouDose = true;
    }
    QVERIFY2(achouGarrafa, "a garrafa tem que aparecer no estoque");
    QVERIFY2(!achouDose, "a dose não pode aparecer no estoque");
}

// Tirar a origem devolve o produto à vida normal, com estoque próprio.
void TstDose::doseSemOrigemVoltaASerProdutoNormal()
{
    QVariantMap d = m_app->produto(m_doseId);
    d[QStringLiteral("doseDeProdutoId")] = 0;
    d[QStringLiteral("doseQuantidade")] = 0;
    QVERIFY2(m_app->salvarProduto(d), qUtf8Printable(m_app->ultimoErro()));

    const auto p = prod().obter(m_doseId);
    QVERIFY(p.has_value());
    QCOMPARE(p->doseDeProdutoId, 0);

    // Agora ele passa a ter estoque próprio (zerado) e aparece no estoque.
    bool achou = false;
    for (const auto &it : estoque().listar()) {
        if (it.produtoId == m_doseId)
            achou = true;
    }
    QVERIFY2(achou, "sem origem, volta a ser produto de estoque");
}

QTEST_MAIN(TstDose)
#include "tst_dose.moc"
