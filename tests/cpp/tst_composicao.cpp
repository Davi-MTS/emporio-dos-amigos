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

// Composição por CATEGORIA: a receita do copão referencia categorias (Destilados
// em ml, Gelo/Energético em unidade). O produto específico é escolhido na venda.
class TstComposicao : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void salvaReceitaPorCategoria();
    void produtosDaCategoriaParaEscolha();
    void venderCopaoBaixaInsumosEscolhidos();
    void custoDoCompostoSomaInsumos();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0, m_sessaoId = 0;
    int m_catDest = 0, m_catGelo = 0, m_catEnerg = 0;
    int m_vodka = 0, m_whisky = 0, m_gelo = 0, m_energ = 0;
    int m_copao = 0, m_embCopao = 0;

    int catId(const QString &nome)
    {
        QSqlQuery q(m_db.connection());
        q.prepare(QStringLiteral("SELECT id FROM categorias WHERE nome = :n"));
        q.bindValue(QStringLiteral(":n"), nome);
        return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
    }
    int criarProduto(const QString &nome, int catId, const QString &base, qint64 preco)
    {
        ProdutoRepository r(m_db.connection());
        Produto p; p.nome = nome; p.categoriaId = catId; p.unidadeBase = base;
        Embalagem e; e.nome = QStringLiteral("Unidade"); e.fator = 1; e.precoVenda = preco;
        p.embalagens = {e};
        r.salvar(p);
        return p.id;
    }
};

void TstComposicao::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery u(m_db.connection());
    QVERIFY(u.exec(QStringLiteral(
        "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) VALUES (1,'A','a','x',1)")));
    m_usuarioId = u.lastInsertId().toInt();

    m_catDest = catId(QStringLiteral("Destilados"));
    m_catGelo = catId(QStringLiteral("Gelo"));
    m_catEnerg = catId(QStringLiteral("Energético"));
    QVERIFY(m_catDest > 0 && m_catGelo > 0 && m_catEnerg > 0);

    // Insumos: vodka/whisky em ml; gelo/energético em unidade.
    m_vodka = criarProduto(QStringLiteral("Vodka"), m_catDest, QStringLiteral("ml"), 0);
    m_whisky = criarProduto(QStringLiteral("Whisky"), m_catDest, QStringLiteral("ml"), 0);
    m_gelo = criarProduto(QStringLiteral("Gelo cubo"), m_catGelo, QStringLiteral("unidade"), 0);
    m_energ = criarProduto(QStringLiteral("Energético lata"), m_catEnerg, QStringLiteral("unidade"), 0);

    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_vodka, 1500, 2, m_usuarioId, QString()));   // 2 c/ml
    QVERIFY(erepo.registrarEntrada(m_whisky, 750, 3, m_usuarioId, QString()));
    QVERIFY(erepo.registrarEntrada(m_gelo, 100, 50, m_usuarioId, QString()));    // 0,50 cada
    QVERIFY(erepo.registrarEntrada(m_energ, 100, 300, m_usuarioId, QString()));  // 3,00 cada

    // Copão = Destilados 50 ml + Gelo 5 un + Energético 1 un, vendido a 15,00.
    ProdutoRepository prepo(m_db.connection());
    Produto c;
    c.nome = QStringLiteral("Copão da Casa");
    c.composto = true;
    Embalagem ec; ec.nome = QStringLiteral("Unidade"); ec.fator = 1; ec.precoVenda = 1500;
    c.embalagens = {ec};
    Componente d; d.categoriaId = m_catDest; d.unidade = QStringLiteral("ml"); d.quantidade = 50;
    Componente g; g.categoriaId = m_catGelo; g.unidade = QStringLiteral("unidade"); g.quantidade = 5;
    Componente e; e.categoriaId = m_catEnerg; e.unidade = QStringLiteral("unidade"); e.quantidade = 1;
    c.composicao = {d, g, e};
    QVERIFY2(prepo.salvar(c), qUtf8Printable(prepo.ultimoErro()));
    m_copao = c.id;
    m_embCopao = c.embalagens.first().id;

    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(0, m_usuarioId);
    QVERIFY(m_sessaoId > 0);
}

void TstComposicao::salvaReceitaPorCategoria()
{
    ProdutoRepository prepo(m_db.connection());
    const auto p = prepo.obter(m_copao);
    QVERIFY(p.has_value());
    QVERIFY(p->composto);
    QCOMPARE(p->composicao.size(), 3);
    // Composto não tem estoque próprio.
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM estoque WHERE produto_id = %1").arg(m_copao)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 0);
}

void TstComposicao::produtosDaCategoriaParaEscolha()
{
    ProdutoRepository prepo(m_db.connection());
    // Destilados tem 2 produtos (vodka, whisky) -> escolha na venda.
    QCOMPARE(prepo.produtosDaCategoria(m_catDest).size(), 2);
    QCOMPARE(prepo.produtosDaCategoria(m_catGelo).size(), 1);
}

void TstComposicao::venderCopaoBaixaInsumosEscolhidos()
{
    VendaRepository vrepo(m_db.connection());
    QVector<LinhaVenda> itens;
    LinhaVenda l; l.produtoId = m_copao; l.embalagemId = m_embCopao; l.fator = 1;
    l.qtdEmbalagem = 2; l.precoUnit = 1500;   // 2 copões
    // Insumos escolhidos na venda: vodka (50 ml), gelo (5), energético (1).
    l.insumos = { {m_vodka, 50}, {m_gelo, 5}, {m_energ, 1} };
    itens.push_back(l);
    QVector<PagamentoVenda> pags;
    PagamentoVenda p; p.forma = QStringLiteral("dinheiro"); p.valor = 3000; pags.push_back(p);

    const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, itens, pags, m_usuarioId);
    QVERIFY2(r.ok, qUtf8Printable(r.erro));

    EstoqueRepository erepo(m_db.connection());
    QCOMPARE(erepo.item(m_vodka).quantidade, qint64(1400)); // 1500 - 2*50
    QCOMPARE(erepo.item(m_gelo).quantidade, qint64(90));    // 100 - 2*5
    QCOMPARE(erepo.item(m_energ).quantidade, qint64(98));   // 100 - 2*1
    QCOMPARE(erepo.item(m_whisky).quantidade, qint64(750)); // não escolhido, intacto
}

void TstComposicao::custoDoCompostoSomaInsumos()
{
    RelatorioRepository rel(m_db.connection());
    const FaturamentoResumo f = rel.faturamento(30);
    QCOMPARE(f.total, qint64(3000));
    // Custo por copão: 50ml*2 + 5*50 + 1*300 = 100 + 250 + 300 = 650. 2 copões = 1300.
    QCOMPARE(f.custo, qint64(1300));
    QCOMPARE(f.lucro, qint64(1700));
}

QTEST_MAIN(TstComposicao)
#include "tst_composicao.moc"
