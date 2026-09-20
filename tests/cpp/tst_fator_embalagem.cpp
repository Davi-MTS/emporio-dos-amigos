#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

// O fator da embalagem vem do CADASTRO, não da tela.
//
// Na loja, compras e vendas foram gravadas com a embalagem certa e o fator
// errado: um BOX de 20 palheiros entrou como 15 unidades a preço de box; uma
// caixinha de 12 latas vendida por R$ 48,00 baixou 1 lata do estoque. A tela
// mandava a embalagem escolhida e o fator separado, e o sistema acreditava no
// fator. O dono corrigia o estoque na mão (inventário +285, −22) e o custo
// continuava errado.
//
// Aqui a "tela" mente de propósito — manda a caixa com fator 1 — e o sistema
// tem que usar o fator 12 do cadastro.
class TstFatorEmbalagem : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void compraUsaOFatorDoCadastro();
    void vendaUsaOFatorDoCadastro();
    void embalagemDeOutroProdutoERecusada();
    void semEmbalagemUsaAUnidadeBase();
    void avaliarCusto_data();
    void avaliarCusto();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_produto = 0, m_unidade = 0, m_caixa = 0;
    int m_outroProduto = 0, m_caixaDoOutro = 0;

    ProdutoRepository prod() { return ProdutoRepository(m_db.connection()); }
    qint64 saldo(int produtoId);
    qint64 custoMedio(int produtoId);   // milésimos de centavo por unidade
    int criarComCaixa(const QString &nome, int *unidade, int *caixa);
    static QVariantMap compra(int produtoId, int embalagemId, int fatorDaTela, int qtd, qint64 custo);
};

qint64 TstFatorEmbalagem::saldo(int produtoId)
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT quantidade_atual FROM estoque WHERE produto_id = :p"));
    q.bindValue(QStringLiteral(":p"), produtoId);
    return (q.exec() && q.next()) ? q.value(0).toLongLong() : 0;
}

qint64 TstFatorEmbalagem::custoMedio(int produtoId)
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT custo_medio_unitario FROM estoque WHERE produto_id = :p"));
    q.bindValue(QStringLiteral(":p"), produtoId);
    return (q.exec() && q.next()) ? q.value(0).toLongLong() : -1;
}

int TstFatorEmbalagem::criarComCaixa(const QString &nome, int *unidade, int *caixa)
{
    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = nome;
    p[QStringLiteral("categoriaId")] =
        m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
    QVariantMap un{{QStringLiteral("id"), 0}, {QStringLiteral("nome"), QStringLiteral("Unidade")},
                   {QStringLiteral("fator"), 1}, {QStringLiteral("codigoBarras"), QString()},
                   {QStringLiteral("preco"), 450}, {QStringLiteral("custo"), -1}};
    QVariantMap cx{{QStringLiteral("id"), 0}, {QStringLiteral("nome"), QStringLiteral("Caixinha")},
                   {QStringLiteral("fator"), 12}, {QStringLiteral("codigoBarras"), QString()},
                   {QStringLiteral("preco"), 4800}, {QStringLiteral("custo"), -1}};
    p[QStringLiteral("embalagens")] = QVariantList{un, cx};
    if (!m_app->salvarProduto(p))
        return 0;
    int id = 0;
    for (const Produto &pr : prod().listar(nome))
        if (pr.nome == nome)
            id = pr.id;
    for (const Embalagem &e : prod().obter(id)->embalagens) {
        if (e.fator == 1) *unidade = e.id;
        if (e.fator == 12) *caixa = e.id;
    }
    return id;
}

QVariantMap TstFatorEmbalagem::compra(int produtoId, int embalagemId, int fatorDaTela, int qtd,
                                      qint64 custo)
{
    QVariantMap item{{QStringLiteral("produtoId"), produtoId},
                     {QStringLiteral("embalagemId"), embalagemId},
                     {QStringLiteral("fator"), fatorDaTela},
                     {QStringLiteral("qtd"), qtd},
                     {QStringLiteral("custo"), custo}};
    return QVariantMap{{QStringLiteral("fornecedorId"), 0},
                       {QStringLiteral("gerarContaPagar"), false},
                       {QStringLiteral("itens"), QVariantList{item}}};
}

void TstFatorEmbalagem::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    m_produto = criarComCaixa(QStringLiteral("Brahma teste"), &m_unidade, &m_caixa);
    m_outroProduto = criarComCaixa(QStringLiteral("Imperio teste"), &m_unidade, &m_caixaDoOutro);
    QVERIFY(m_produto > 0 && m_outroProduto > 0 && m_caixa > 0 && m_caixaDoOutro > 0);
    // m_unidade ficou com a do segundo produto; relê a do primeiro.
    for (const Embalagem &e : prod().obter(m_produto)->embalagens)
        if (e.fator == 1) m_unidade = e.id;

    QVERIFY(m_app->abrirCaixa(QStringLiteral("0,00")));
}

// O caso do BOX de palheiro: a tela manda a caixa com fator 1.
void TstFatorEmbalagem::compraUsaOFatorDoCadastro()
{
    const QVariantMap r = m_app->registrarCompra(
        compra(m_produto, m_caixa, /*fator mentindo=*/1, /*caixas=*/2, /*R$ 36,00 a caixa=*/3600));
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    QCOMPARE(saldo(m_produto), Q_INT64_C(24));             // 2 caixas × 12, e não 2
    QCOMPARE(custoMedio(m_produto), Q_INT64_C(300000));    // R$ 3,00 a lata, e não R$ 36,00
}

// O caso da caixinha de IMPERIO: vendida por R$ 48,00 com fator 1.
void TstFatorEmbalagem::vendaUsaOFatorDoCadastro()
{
    const qint64 antes = saldo(m_produto);

    QVariantMap item{{QStringLiteral("produtoId"), m_produto},
                     {QStringLiteral("embalagemId"), m_caixa},
                     {QStringLiteral("fator"), 1},             // a tela mentindo
                     {QStringLiteral("qtd"), 1},
                     {QStringLiteral("precoUnit"), 4800},
                     {QStringLiteral("desconto"), 0}};
    QVariantMap pag{{QStringLiteral("forma"), QStringLiteral("dinheiro")},
                    {QStringLiteral("valor"), 4800}};
    QVariantMap venda{{QStringLiteral("desconto"), 0},
                      {QStringLiteral("clienteId"), 0},
                      {QStringLiteral("itens"), QVariantList{item}},
                      {QStringLiteral("pagamentos"), QVariantList{pag}}};
    const QVariantMap r = m_app->finalizarVenda(venda);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    QCOMPARE(saldo(m_produto), antes - 12);   // a caixinha inteira, e não 1 lata
}

// A embalagem de um produto não pode ser usada em outro: o fator viria de
// uma coisa que não é o que está sendo vendido ou comprado.
void TstFatorEmbalagem::embalagemDeOutroProdutoERecusada()
{
    const qint64 antes = saldo(m_produto);
    const QVariantMap r = m_app->registrarCompra(compra(m_produto, m_caixaDoOutro, 12, 1, 3600));
    QCOMPARE(r.value(QStringLiteral("ok")).toBool(), false);
    QVERIFY(r.value(QStringLiteral("erro")).toString().contains(QStringLiteral("embalagem")));
    QCOMPARE(saldo(m_produto), antes);   // nada entrou
}

// Sem embalagem (o copão manda 0) continua valendo a unidade base — e o fator
// que a tela mandar não interfere.
void TstFatorEmbalagem::semEmbalagemUsaAUnidadeBase()
{
    const qint64 antes = saldo(m_produto);
    const QVariantMap r = m_app->registrarCompra(compra(m_produto, 0, /*fator mentindo=*/12, 5, 300));
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));
    QCOMPARE(saldo(m_produto), antes + 5);
}

// Aviso de custo fora do normal. Unidade a R$ 4,50 → a caixinha de 12 rende
// R$ 54,00. Acima do que rende = "alto"; abaixo de 10% = "baixo".
// Os casos "baixo" e "alto" são os erros vistos na loja: custo da lata lançado
// na caixinha, e custo da caixinha lançado na lata.
void TstFatorEmbalagem::avaliarCusto_data()
{
    QTest::addColumn<bool>("naCaixa");
    QTest::addColumn<QString>("custo");
    QTest::addColumn<QString>("nivel");

    QTest::newRow("caixinha no preço certo")        << true  << "36,00" << "";
    QTest::newRow("caixinha com custo de 1 lata")   << true  << "3,00"  << "baixo";
    QTest::newRow("caixinha acima do que rende")    << true  << "60,00" << "alto";
    QTest::newRow("lata no preço certo")            << false << "3,00"  << "";
    QTest::newRow("lata com custo da caixinha")     << false << "36,00" << "alto";
    QTest::newRow("lata com custo irrisório")       << false << "0,40"  << "baixo";
    QTest::newRow("custo vazio não avisa")          << true  << ""      << "";
    QTest::newRow("custo inválido não avisa")       << true  << "abc"   << "";
}

void TstFatorEmbalagem::avaliarCusto()
{
    QFETCH(bool, naCaixa);
    QFETCH(QString, custo);
    QFETCH(QString, nivel);

    const QVariantMap r = m_app->avaliarCusto(m_produto, naCaixa ? m_caixa : m_unidade, custo);
    QCOMPARE(r.value(QStringLiteral("nivel")).toString(), nivel);
    QCOMPARE(r.value(QStringLiteral("mensagem")).toString().isEmpty(), nivel.isEmpty());
}

QTEST_MAIN(TstFatorEmbalagem)
#include "tst_fator_embalagem.moc"
