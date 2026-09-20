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

class TstRelatorioRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void dashboardConfere();
    void faturamentoELucro();
    void vendasPorFormaEMaisVendidos();
    void produtosParados();
    void lucroTravadoNoMomentoDaVenda();
    void diaEspecifico();
    void maisVendidosNaoMisturaMlComUnidade();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0, m_sessaoId = 0;
    int m_prodA = 0, m_embA = 0, m_prodB = 0;
    RelatorioRepository rel() { return RelatorioRepository(m_db.connection()); }
};

void TstRelatorioRepository::initTestCase()
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
    Produto a; a.nome = QStringLiteral("Produto A"); a.estoqueMinimo = 0;
    Embalagem ea; ea.nome = QStringLiteral("Unidade"); ea.fator = 1; ea.precoVenda = 500;
    a.embalagens = {ea};
    QVERIFY(prepo.salvar(a)); m_prodA = a.id; m_embA = a.embalagens.first().id;

    Produto b; b.nome = QStringLiteral("Produto B"); b.estoqueMinimo = 1000; // ficará em falta
    Embalagem eb; eb.nome = QStringLiteral("Unidade"); eb.fator = 1; eb.precoVenda = 900;
    b.embalagens = {eb};
    QVERIFY(prepo.salvar(b)); m_prodB = b.id;

    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_prodA, 100, 300, m_usuarioId, QString())); // custo 3,00
    QVERIFY(erepo.registrarEntrada(m_prodB, 10, 500, m_usuarioId, QString()));

    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(0, m_usuarioId);

    VendaRepository vrepo(m_db.connection());
    // Venda 1: 3 un de A (15,00) em dinheiro.
    {
        QVector<LinhaVenda> it; LinhaVenda l; l.produtoId=m_prodA; l.embalagemId=m_embA; l.fator=1; l.qtdEmbalagem=3; l.precoUnit=500; it.push_back(l);
        QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma="dinheiro"; p.valor=1500; pg.push_back(p);
        QVERIFY(vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId).ok);
    }
    // Venda 2: 2 un de A (10,00) em dinheiro.
    {
        QVector<LinhaVenda> it; LinhaVenda l; l.produtoId=m_prodA; l.embalagemId=m_embA; l.fator=1; l.qtdEmbalagem=2; l.precoUnit=500; it.push_back(l);
        QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma="dinheiro"; p.valor=1000; pg.push_back(p);
        QVERIFY(vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId).ok);
    }
}

void TstRelatorioRepository::dashboardConfere()
{
    const DashboardKpis k = rel().dashboard();
    QCOMPARE(k.vendasHoje, qint64(2500));
    QCOMPARE(k.numVendasHoje, 2);
    QCOMPARE(k.ticketMedio, qint64(1250));
    QCOMPARE(k.produtosEmFalta, 1);   // Produto B
}

void TstRelatorioRepository::faturamentoELucro()
{
    const FaturamentoResumo f = rel().faturamento(0); // hoje
    QCOMPARE(f.total, qint64(2500));
    QCOMPARE(f.numVendas, 2);
    // Custo: 5 un de A * 3,00 = 15,00. Lucro = 25,00 - 15,00 = 10,00.
    QCOMPARE(f.custo, qint64(1500));
    QCOMPARE(f.lucro, qint64(1000));
}

void TstRelatorioRepository::vendasPorFormaEMaisVendidos()
{
    const auto formas = rel().vendasPorForma(0);
    QCOMPARE(formas.size(), 1);
    QCOMPARE(formas.first().forma, QStringLiteral("dinheiro"));
    QCOMPARE(formas.first().total, qint64(2500));

    const auto top = rel().maisVendidos(0, 5);
    QVERIFY(!top.isEmpty());
    QCOMPARE(top.first().nome, QStringLiteral("Produto A"));
    QCOMPARE(top.first().qtd, qint64(5));
}

void TstRelatorioRepository::produtosParados()
{
    const auto parados = rel().produtosParados(0);
    // Produto B não vendeu hoje.
    bool achouB = false;
    for (const ProdutoParado &p : parados)
        if (p.nome == QStringLiteral("Produto B")) achouB = true;
    QVERIFY(achouB);
    // Produto A vendeu, não deve estar parado.
    for (const ProdutoParado &p : parados)
        QVERIFY(p.nome != QStringLiteral("Produto A"));
}

void TstRelatorioRepository::lucroTravadoNoMomentoDaVenda()
{
    // As 5 unidades de A foram vendidas com custo médio 3,00 (agora travado na
    // movimentação). Uma nova entrada bem mais cara eleva o custo médio atual.
    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_prodA, 100, 900, m_usuarioId, QString())); // 9,00/un

    // O lucro das vendas de hoje NÃO muda: continua valendo 3,00/un (custo do
    // momento), e não o novo custo médio.
    const FaturamentoResumo f = rel().faturamento(0);
    QCOMPARE(f.total, qint64(2500));
    QCOMPARE(f.custo, qint64(1500));   // 5 * 3,00 travado
    QCOMPARE(f.lucro, qint64(1000));
}

// "Quanto vendi ontem?" A receita vem de `vendas.data` e o custo de
// `movimentacoes_estoque.data` — duas tabelas. Se o filtro do dia pegasse só uma
// delas, o lucro do dia sairia com a receita de um dia e o custo de outro.
void TstRelatorioRepository::diaEspecifico()
{
    const FaturamentoResumo hojeAntes = rel().faturamento(0);

    // Custo médio de A NESTE momento. Não é mais 3,00: o teste anterior deu
    // entrada a 9,00. É esse custo que a venda abaixo trava.
    QSqlQuery c(m_db.connection());
    QVERIFY(c.exec(QStringLiteral("SELECT custo_medio_unitario FROM estoque WHERE produto_id = %1").arg(m_prodA)));
    QVERIFY(c.next());
    const qint64 custoEsperado = 4 * c.value(0).toLongLong() / 1000;

    // Uma venda nova (4 un de A a 5,00 = 20,00), que depois é empurrada para
    // ONTEM junto com a baixa de estoque dela.
    VendaRepository vrepo(m_db.connection());
    QVector<LinhaVenda> it; LinhaVenda l; l.produtoId = m_prodA; l.embalagemId = m_embA;
    l.fator = 1; l.qtdEmbalagem = 4; l.precoUnit = 500; it.push_back(l);
    QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma = QStringLiteral("pix"); p.valor = 2000; pg.push_back(p);
    const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId);
    QVERIFY2(r.ok, qPrintable(r.erro));

    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("UPDATE vendas SET data = datetime(data, '-1 day') WHERE id = %1").arg(r.vendaId)));
    QVERIFY(q.exec(QStringLiteral("UPDATE movimentacoes_estoque SET data = datetime(data, '-1 day') "
                                  "WHERE origem = 'venda:%1'").arg(r.vendaId)));

    const QDate hoje = QDate::currentDate();
    const FaturamentoResumo ontem = rel().faturamento(Periodo::doDia(hoje.addDays(-1)));
    QCOMPARE(ontem.total, qint64(2000));
    QCOMPARE(ontem.numVendas, 1);
    QVERIFY(custoEsperado > 0);
    QCOMPARE(ontem.custo, custoEsperado);   // o custo da venda veio junto para ontem
    QCOMPARE(ontem.lucro, qint64(2000) - custoEsperado);

    // Hoje não ganhou a venda de ontem.
    const FaturamentoResumo hojeDepois = rel().faturamento(0);
    QCOMPARE(hojeDepois.total, hojeAntes.total);
    QCOMPARE(hojeDepois.lucro, hojeAntes.lucro);

    // O dia específico de hoje é o mesmo que o botão "Hoje".
    const FaturamentoResumo hojePorData = rel().faturamento(Periodo::doDia(hoje));
    QCOMPARE(hojePorData.total, hojeDepois.total);
    QCOMPARE(hojePorData.lucro, hojeDepois.lucro);

    // Anteontem: nada.
    QCOMPARE(rel().faturamento(Periodo::doDia(hoje.addDays(-2))).numVendas, 0);

    // As listas também respeitam o dia.
    const auto formasOntem = rel().vendasPorForma(Periodo::doDia(hoje.addDays(-1)));
    QCOMPARE(formasOntem.size(), 1);
    QCOMPARE(formasOntem.first().forma, QStringLiteral("pix"));
    QCOMPARE(rel().maisVendidos(Periodo::doDia(hoje.addDays(-1)), 10).first().qtd, qint64(4));
}

// Somar unidade base punha 2000 ml de whisky (2 garrafas) acima de 5 latas.
void TstRelatorioRepository::maisVendidosNaoMisturaMlComUnidade()
{
    ProdutoRepository prepo(m_db.connection());
    Produto w; w.nome = QStringLiteral("Whisky 1L"); w.unidadeBase = QStringLiteral("ml");
    Embalagem g; g.nome = QStringLiteral("Garrafa"); g.fator = 1000; g.precoVenda = 9000;
    w.embalagens = {g};
    QVERIFY2(prepo.salvar(w), qUtf8Printable(prepo.ultimoErro()));

    VendaRepository vrepo(m_db.connection());
    QVector<LinhaVenda> it; LinhaVenda l; l.produtoId = w.id; l.embalagemId = w.embalagens.first().id;
    l.fator = 1000; l.qtdEmbalagem = 2; l.precoUnit = 9000; it.push_back(l);
    QVector<PagamentoVenda> pg; PagamentoVenda p; p.forma = QStringLiteral("pix"); p.valor = 18000; pg.push_back(p);
    QVERIFY(vrepo.registrarVenda(m_sessaoId, 0, 0, it, pg, m_usuarioId).ok);

    const auto top = rel().maisVendidos(0, 10);
    int posWhisky = -1, posA = -1;
    for (int i = 0; i < top.size(); ++i) {
        if (top.at(i).nome == QStringLiteral("Whisky 1L")) { posWhisky = i; QCOMPARE(top.at(i).qtd, qint64(2)); }
        if (top.at(i).qtd >= 5 && posA < 0) posA = i;
    }
    QVERIFY(posWhisky >= 0);
    QVERIFY2(posA >= 0 && posA < posWhisky, "2 garrafas apareceram acima de 5 unidades");
}

QTEST_MAIN(TstRelatorioRepository)
#include "tst_relatorio_repository.moc"
