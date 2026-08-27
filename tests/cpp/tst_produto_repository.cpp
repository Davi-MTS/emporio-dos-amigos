#include <QtTest>

#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/produtos/ProdutoRepository.h"

class TstProdutoRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void criaListaEObtem();
    void buscaPorCodigoBarras();
    void atualizaRemovendoEmbalagem();
    void inativaSomeDaLista();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_produtoId = 0;

    ProdutoRepository repo() { return ProdutoRepository(m_db.connection()); }
};

void TstProdutoRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))),
             qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));
}

void TstProdutoRepository::criaListaEObtem()
{
    auto r = repo();

    const auto cats = r.listarCategorias();
    QVERIFY(!cats.isEmpty()); // seed populou categorias

    Produto p;
    p.nome = QStringLiteral("Heineken Long Neck 330ml");
    p.categoriaId = cats.first().first;
    p.unidadeBase = QStringLiteral("long neck");
    p.estoqueMinimo = 120;

    Embalagem unidade;
    unidade.nome = QStringLiteral("Unidade");
    unidade.fator = 1;
    unidade.codigoBarras = QStringLiteral("111");
    unidade.precoVenda = 549;

    Embalagem caixa;
    caixa.nome = QStringLiteral("Caixa");
    caixa.fator = 12;
    caixa.codigoBarras = QStringLiteral("222");
    caixa.precoVenda = 6290;

    p.embalagens = {unidade, caixa};

    QVERIFY2(r.salvar(p), qUtf8Printable(r.ultimoErro()));
    QVERIFY(p.id > 0);
    m_produtoId = p.id;
    // ids das embalagens preenchidos.
    QVERIFY(p.embalagens.at(0).id > 0);
    QVERIFY(p.embalagens.at(1).id > 0);

    // Listagem traz o preço principal (menor fator) e estoque zerado.
    const auto lista = r.listar();
    bool achou = false;
    for (const Produto &x : lista) {
        if (x.id == m_produtoId) {
            achou = true;
            QCOMPARE(x.precoPrincipal, qint64(549));
            QCOMPARE(x.quantidadeEstoque, qint64(0));
            QVERIFY(!x.categoriaNome.isEmpty());
        }
    }
    QVERIFY(achou);

    // obter() traz as embalagens completas.
    const auto obtido = r.obter(m_produtoId);
    QVERIFY(obtido.has_value());
    QCOMPARE(obtido->embalagens.size(), 2);
}

void TstProdutoRepository::buscaPorCodigoBarras()
{
    auto r = repo();
    const auto achado = r.buscarPorCodigoBarras(QStringLiteral("222"));
    QVERIFY(achado.has_value());
    QCOMPARE(achado->first.id, m_produtoId);
    QCOMPARE(achado->second.fator, 12);
    QCOMPARE(achado->second.precoVenda, qint64(6290));

    QVERIFY(!r.buscarPorCodigoBarras(QStringLiteral("inexistente")).has_value());
}

void TstProdutoRepository::atualizaRemovendoEmbalagem()
{
    auto r = repo();
    auto p = r.obter(m_produtoId);
    QVERIFY(p.has_value());

    p->nome = QStringLiteral("Heineken 330ml (novo nome)");
    // Mantém só a embalagem de fator 1.
    QVector<Embalagem> apenasBase;
    for (const Embalagem &e : p->embalagens) {
        if (e.fator == 1)
            apenasBase.push_back(e);
    }
    p->embalagens = apenasBase;

    QVERIFY2(r.salvar(*p), qUtf8Printable(r.ultimoErro()));

    const auto recarregado = r.obter(m_produtoId);
    QVERIFY(recarregado.has_value());
    QCOMPARE(recarregado->embalagens.size(), 1);
    QCOMPARE(recarregado->nome, QStringLiteral("Heineken 330ml (novo nome)"));
    // A embalagem "Caixa" (código 222) não existe mais.
    QVERIFY(!r.buscarPorCodigoBarras(QStringLiteral("222")).has_value());
}

void TstProdutoRepository::inativaSomeDaLista()
{
    auto r = repo();
    QVERIFY2(r.inativar(m_produtoId), qUtf8Printable(r.ultimoErro()));

    const auto lista = r.listar();
    for (const Produto &x : lista)
        QVERIFY(x.id != m_produtoId);
}

QTEST_MAIN(TstProdutoRepository)
#include "tst_produto_repository.moc"
