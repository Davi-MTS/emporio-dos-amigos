#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/compras/CompraRepository.h"
#include "domain/compras/FornecedorRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"

class TstCompraRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void fornecedorCrud();
    void compraEntraNoEstoqueEGeraContaPagar();
    void segundaCompraAtualizaCustoMedio();
    void custoSubCentavoPreservado();
    void compraComNotaFiscal();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0;
    int m_fornId = 0;
    int m_produtoId = 0;
    int m_embCaixaId = 0;
    int m_compraId = 0;
};

void TstCompraRepository::initTestCase()
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
    Produto p;
    p.nome = QStringLiteral("Produto Compra");
    Embalagem base; base.nome = QStringLiteral("Unidade"); base.fator = 1; base.precoVenda = 800;
    Embalagem cx; cx.nome = QStringLiteral("Caixa"); cx.fator = 12; cx.precoVenda = 9000;
    p.embalagens = {base, cx};
    QVERIFY2(prepo.salvar(p), qUtf8Printable(prepo.ultimoErro()));
    m_produtoId = p.id;
    for (const Embalagem &e : p.embalagens)
        if (e.fator == 12) m_embCaixaId = e.id;
    QVERIFY(m_embCaixaId > 0);
}

void TstCompraRepository::fornecedorCrud()
{
    FornecedorRepository r(m_db.connection());
    Fornecedor f;
    f.nome = QStringLiteral("Bebidas Sul");
    f.cnpj = QStringLiteral("12.345.678/0001-99");
    f.telefone = QStringLiteral("51 99999-0000");
    QVERIFY2(r.salvar(f), qUtf8Printable(r.ultimoErro()));
    QVERIFY(f.id > 0);
    m_fornId = f.id;

    const auto obtido = r.obter(m_fornId);
    QVERIFY(obtido.has_value());
    QCOMPARE(obtido->nome, QStringLiteral("Bebidas Sul"));
    QCOMPARE(r.listar().size(), 1);
}

void TstCompraRepository::compraEntraNoEstoqueEGeraContaPagar()
{
    CompraRepository crepo(m_db.connection());

    // Compra 5 caixas (fator 12) a 60,00/caixa -> custo base 5,00.
    QVector<ItemCompra> itens;
    ItemCompra it; it.produtoId = m_produtoId; it.embalagemId = m_embCaixaId; it.fator = 12;
    it.qtdEmbalagem = 5; it.custoUnitEmbalagem = 6000; itens.push_back(it);

    const ResultadoCompra r = crepo.registrarCompra(m_fornId, QStringLiteral("manual"),
                                                    itens, true, QStringLiteral("2026-09-01"),
                                                    m_usuarioId);
    QVERIFY2(r.ok, qUtf8Printable(r.erro));
    QCOMPARE(r.total, qint64(30000)); // 5 * 60,00
    m_compraId = r.compraId;

    // Estoque: 60 unidades base, custo médio 5,00 (6000/12).
    EstoqueRepository erepo(m_db.connection());
    const ItemEstoque ie = erepo.item(m_produtoId);
    QCOMPARE(ie.quantidade, qint64(60));
    QCOMPARE(ie.custoMedio, qint64(500));

    // Conta a pagar criada.
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "SELECT valor, status FROM contas_pagar WHERE compra_id = %1").arg(m_compraId)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toLongLong(), qint64(30000));
    QCOMPARE(q.value(1).toString(), QStringLiteral("aberta"));

    // Compra listada com fornecedor.
    const auto lista = crepo.listar();
    QVERIFY(!lista.isEmpty());
    QCOMPARE(lista.first().fornecedorNome, QStringLiteral("Bebidas Sul"));
    QCOMPARE(lista.first().numItens, 1);
}

void TstCompraRepository::segundaCompraAtualizaCustoMedio()
{
    CompraRepository crepo(m_db.connection());
    // +3 caixas a 72,00 -> base 6,00; qtdBase 36.
    QVector<ItemCompra> itens;
    ItemCompra it; it.produtoId = m_produtoId; it.embalagemId = m_embCaixaId; it.fator = 12;
    it.qtdEmbalagem = 3; it.custoUnitEmbalagem = 7200; itens.push_back(it);
    const ResultadoCompra r = crepo.registrarCompra(m_fornId, QStringLiteral("manual"),
                                                    itens, false, QString(), m_usuarioId);
    QVERIFY2(r.ok, qUtf8Printable(r.erro));

    // (60*500 + 36*600) / 96 = 51600/96 = 537 (trunc).
    EstoqueRepository erepo(m_db.connection());
    const ItemEstoque ie = erepo.item(m_produtoId);
    QCOMPARE(ie.quantidade, qint64(96));
    QCOMPARE(ie.custoMedio, qint64(537));
}

void TstCompraRepository::custoSubCentavoPreservado()
{
    // Produto medido em ml: garrafa de 1000 ml (fator 1000).
    ProdutoRepository prepo(m_db.connection());
    Produto v; v.nome = QStringLiteral("Vodka ml"); v.unidadeBase = QStringLiteral("ml");
    Embalagem g; g.nome = QStringLiteral("Garrafa"); g.fator = 1000; g.precoVenda = 0;
    v.embalagens = {g};
    QVERIFY2(prepo.salvar(v), qUtf8Printable(prepo.ultimoErro()));
    const int vid = v.id;
    const int embG = v.embalagens.first().id;

    // Compra 1 garrafa a R$13,33 -> custo por ml = 1,333 centavos.
    CompraRepository crepo(m_db.connection());
    QVector<ItemCompra> itens;
    ItemCompra it; it.produtoId = vid; it.embalagemId = embG; it.fator = 1000;
    it.qtdEmbalagem = 1; it.custoUnitEmbalagem = 1333; itens.push_back(it);
    QVERIFY(crepo.registrarCompra(0, QStringLiteral("manual"), itens, false, QString(), m_usuarioId).ok);

    // O custo por ml é guardado em MILÉSIMOS: 1333 (não arredonda para 0/1).
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "SELECT custo_medio_unitario FROM estoque WHERE produto_id = %1").arg(vid)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toLongLong(), qint64(1333));
}

void TstCompraRepository::compraComNotaFiscal()
{
    CompraRepository crepo(m_db.connection());
    QVector<ItemCompra> itens;
    ItemCompra it; it.produtoId = m_produtoId; it.embalagemId = m_embCaixaId; it.fator = 12;
    it.qtdEmbalagem = 1; it.custoUnitEmbalagem = 6000; itens.push_back(it);

    const ResultadoCompra r = crepo.registrarCompra(
        m_fornId, QStringLiteral("manual"), itens, false, QString(), m_usuarioId,
        QStringLiteral("123456"), QStringLiteral("2026-08-20"));
    QVERIFY2(r.ok, qUtf8Printable(r.erro));

    // Nº/data da nota gravados e origem marcada como 'nota'.
    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "SELECT numero_nota, data_nota, origem FROM compras WHERE id = %1").arg(r.compraId)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("123456"));
    QCOMPARE(q.value(1).toString(), QStringLiteral("2026-08-20"));
    QCOMPARE(q.value(2).toString(), QStringLiteral("nota"));

    // Aparece na listagem com o nº da nota.
    const auto lista = crepo.listar();
    QVERIFY(!lista.isEmpty());
    QCOMPARE(lista.first().numeroNota, QStringLiteral("123456"));
}

QTEST_MAIN(TstCompraRepository)
#include "tst_compra_repository.moc"
