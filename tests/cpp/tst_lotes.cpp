#include <QtTest>

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "app/AppBackend.h"
#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/lotes/LoteRepository.h"
#include "domain/compras/CompraRepository.h"
#include "domain/produtos/ProdutoRepository.h"

// Validade por remessa. O que precisa estar certo:
//  · a saída consome o lote que vence PRIMEIRO (FEFO). Ao contrário, a caixa
//    velha encalha na prateleira e vira prejuízo com o sistema dizendo que está
//    tudo bem;
//  · lote zerado some, senão a tela de vencimento enche de linha morta;
//  · quando parte da mercadoria entrou SEM validade, o total dos lotes fica
//    menor que o estoque — isso não é erro, e o sistema precisa saber apontar
//    em vez de fingir que os números batem.
class TstLotes : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void entradaComValidadeCriaLote();
    void saidaConsomeOQueVencePrimeiro();
    void loteZeradoSomeDaLista();
    void resumoSeparaVencidoDeAVencer();
    void entradaSemValidadeApareceComoDivergencia();
    void compraComValidadeCriaLote();

private:
    QTemporaryDir m_dir;
    Database m_db;
    QScopedPointer<AppBackend> m_app;
    int m_produtoId = 0;
    int m_embalagemId = 0;

    ProdutoRepository prod() { return ProdutoRepository(m_db.connection()); }
    EstoqueRepository estoque() { return EstoqueRepository(m_db.connection()); }
    LoteRepository lotes() { return LoteRepository(m_db.connection()); }
    QString emDias(int dias) const
    {
        return QDate::currentDate().addDays(dias).toString(Qt::ISODate);
    }
};

void TstLotes::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    m_app.reset(new AppBackend(m_db.connection()));
    QVERIFY(m_app->criarAdmin(QStringLiteral("Dono"), QStringLiteral("dono"),
                              QStringLiteral("dono12345")));
    QVERIFY(m_app->login(QStringLiteral("dono"), QStringLiteral("dono12345")));

    QVariantMap p = m_app->novoProduto();
    p[QStringLiteral("nome")] = QStringLiteral("Chocolate Teste");
    p[QStringLiteral("categoriaId")] =
        m_app->categorias().first().toMap().value(QStringLiteral("id")).toInt();
    QVERIFY2(m_app->salvarProduto(p), qUtf8Printable(m_app->ultimoErro()));
    for (const Produto &pr : prod().listar(QStringLiteral("Chocolate Teste")))
        m_produtoId = pr.id;
    QVERIFY(m_produtoId > 0);
    m_embalagemId = prod().obter(m_produtoId)->embalagens.first().id;

    QVERIFY(m_app->abrirCaixa(QStringLiteral("50,00")));
}

void TstLotes::entradaComValidadeCriaLote()
{
    // Carga velha: 10 unidades vencendo em 5 dias.
    QVERIFY2(m_app->registrarEntrada(m_produtoId, m_embalagemId, 10, QStringLiteral("20,00"),
                                     QStringLiteral("carga antiga"), emDias(5),
                                     QStringLiteral("L-ANTIGO")),
             qUtf8Printable(m_app->ultimoErro()));
    // Carga nova: 20 unidades vencendo em 60 dias.
    QVERIFY(m_app->registrarEntrada(m_produtoId, m_embalagemId, 20, QStringLiteral("40,00"),
                                    QStringLiteral("carga nova"), emDias(60),
                                    QStringLiteral("L-NOVO")));

    QCOMPARE(estoque().item(m_produtoId).quantidade, qint64(30));
    QCOMPARE(lotes().totalEmLotes(m_produtoId), qint64(30));

    const auto lista = lotes().listar(-1);
    QCOMPARE(lista.size(), 2);
    // Ordenado por validade: o que vence antes vem primeiro.
    QCOMPARE(lista.at(0).codigo, QStringLiteral("L-ANTIGO"));
    QCOMPARE(lista.at(0).quantidade, qint64(10));
    QCOMPARE(lista.at(1).codigo, QStringLiteral("L-NOVO"));
}

// O coração da regra: vender tira da caixa que vence primeiro.
void TstLotes::saidaConsomeOQueVencePrimeiro()
{
    QVariantMap item;
    item[QStringLiteral("produtoId")] = m_produtoId;
    item[QStringLiteral("embalagemId")] = m_embalagemId;
    item[QStringLiteral("fator")] = 1;
    item[QStringLiteral("qtd")] = 4;
    item[QStringLiteral("precoUnit")] = 500;
    item[QStringLiteral("desconto")] = 0;

    QVariantMap pag;
    pag[QStringLiteral("forma")] = QStringLiteral("dinheiro");
    pag[QStringLiteral("valor")] = 2000;

    QVariantMap venda;
    venda[QStringLiteral("desconto")] = 0;
    venda[QStringLiteral("clienteId")] = 0;
    venda[QStringLiteral("itens")] = QVariantList{item};
    venda[QStringLiteral("pagamentos")] = QVariantList{pag};

    const QVariantMap r = m_app->finalizarVenda(venda);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    const auto lista = lotes().listar(-1);
    QCOMPARE(lista.size(), 2);
    QCOMPARE(lista.at(0).codigo, QStringLiteral("L-ANTIGO"));
    QCOMPARE(lista.at(0).quantidade, qint64(6));    // 10 - 4, saiu do mais velho
    QCOMPARE(lista.at(1).quantidade, qint64(20));   // o novo não foi tocado
    QCOMPARE(estoque().item(m_produtoId).quantidade, qint64(26));
}

// Zerar o lote velho: ele some, e o excedente vai para o próximo.
void TstLotes::loteZeradoSomeDaLista()
{
    // Retirada de 8: consome os 6 que restam do velho e 2 do novo.
    QVERIFY2(m_app->registrarRetirada(m_produtoId, m_embalagemId, 8, QStringLiteral("quebra")),
             qUtf8Printable(m_app->ultimoErro()));

    const auto lista = lotes().listar(-1);
    QCOMPARE(lista.size(), 1);
    QCOMPARE(lista.at(0).codigo, QStringLiteral("L-NOVO"));
    QCOMPARE(lista.at(0).quantidade, qint64(18));
    QCOMPARE(lotes().totalEmLotes(m_produtoId), qint64(18));
    QCOMPARE(estoque().item(m_produtoId).quantidade, qint64(18));
}

void TstLotes::resumoSeparaVencidoDeAVencer()
{
    // Uma remessa que JÁ venceu ontem.
    QVERIFY(m_app->registrarEntrada(m_produtoId, m_embalagemId, 3, QString(),
                                    QStringLiteral("esquecida"), emDias(-1),
                                    QStringLiteral("L-VENCIDO")));

    const ResumoVencimento r = lotes().resumo();
    QCOMPARE(r.vencidos, 1);
    QCOMPARE(r.quantidadeVencida, qint64(3));
    QCOMPARE(r.venceEm7, 0);     // o de 5 dias já foi todo consumido
    QCOMPARE(r.venceEm30, 0);    // o que resta vence em 60 dias

    // O filtro "só vencidos" (dias = 0) traz apenas ele.
    const auto soVencidos = lotes().listar(0);
    QCOMPARE(soVencidos.size(), 1);
    QCOMPARE(soVencidos.at(0).codigo, QStringLiteral("L-VENCIDO"));
    QVERIFY(soVencidos.at(0).diasParaVencer < 0);
}

// Entrada sem validade não cria lote: o estoque sobe e os lotes não. O sistema
// tem que APONTAR essa diferença em vez de deixar o dono achar que sumiu coisa.
void TstLotes::entradaSemValidadeApareceComoDivergencia()
{
    QCOMPARE(lotes().divergencias().size(), 0);   // até aqui bate

    QVERIFY(m_app->registrarEntrada(m_produtoId, m_embalagemId, 7, QString(),
                                    QStringLiteral("sem validade"), QString(), QString()));

    const auto div = lotes().divergencias();
    QCOMPARE(div.size(), 1);
    QCOMPARE(div.at(0).first, QStringLiteral("Chocolate Teste"));
    QCOMPARE(div.at(0).second, qint64(7));   // 7 unidades fora do controle de validade
}

// A mercadoria costuma entrar pela COMPRA, não pela tela de estoque. Se a
// validade da nota não virar lote ali, o controle de vencimento fica vazio
// justamente no caminho que a loja usa todo dia.
void TstLotes::compraComValidadeCriaLote()
{
    const qint64 antes = lotes().totalEmLotes(m_produtoId);

    QVariantMap item;
    item[QStringLiteral("produtoId")] = m_produtoId;
    item[QStringLiteral("embalagemId")] = m_embalagemId;
    item[QStringLiteral("fator")] = 1;
    item[QStringLiteral("qtd")] = 24;
    item[QStringLiteral("custo")] = 250;
    item[QStringLiteral("validade")] = emDias(45);
    item[QStringLiteral("lote")] = QStringLiteral("NF-777");

    QVariantMap compra;
    compra[QStringLiteral("fornecedorId")] = 0;
    compra[QStringLiteral("gerarContaPagar")] = false;
    compra[QStringLiteral("itens")] = QVariantList{item};

    const QVariantMap r = m_app->registrarCompra(compra);
    QVERIFY2(r.value(QStringLiteral("ok")).toBool(),
             qUtf8Printable(r.value(QStringLiteral("erro")).toString()));

    QCOMPARE(lotes().totalEmLotes(m_produtoId), antes + 24);

    bool achou = false;
    for (const Lote &l : lotes().listar(-1)) {
        if (l.codigo == QStringLiteral("NF-777")) {
            achou = true;
            QCOMPARE(l.quantidade, qint64(24));
            QVERIFY(l.diasParaVencer > 40);
        }
    }
    QVERIFY2(achou, "o lote da compra nao apareceu no controle de vencimento");
}

QTEST_MAIN(TstLotes)
#include "tst_lotes.moc"
