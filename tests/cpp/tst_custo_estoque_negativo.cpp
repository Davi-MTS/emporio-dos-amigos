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

// O lucro do Dashboard "caía de uma vez e depois voltava a subir", na loja.
//
// O PDV deixa vender sem estoque (só avisa), e numa loja que acabou de começar
// isso é o normal: vende antes de lançar a mercadoria e o saldo fica negativo.
// Aí chega a compra, e o custo médio ponderado era calculado assim:
//
//     (qtd_atual × custo_atual + qtd_nova × custo_novo) ÷ (qtd_atual + qtd_nova)
//
// Com qtd_atual NEGATIVA o divisor fica pequeno e o custo explode. Vendeu 20
// latas sem estoque e comprou 24 a R$ 3,00: o divisor vira 4 e cada lata passa a
// "custar" R$ 18,00. As vendas seguintes gravam esse custo, dão prejuízo e o
// lucro despenca; a compra seguinte dilui a média e ele volta a subir.
//
// As unidades negativas já foram vendidas — não existem mais na prateleira e
// não carregam custo. O custo de quem está chegando é o custo da compra.
class TstCustoEstoqueNegativo : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void compraDepoisDeVenderSemEstoque();
    void entradaSemCustoComSaldoNegativoNaoZera();
    void mediaNormalContinuaPonderada();
    void estoqueSemCustoNaoEntraNaMedia();
    void vendaSemEstoqueGanhaCustoQuandoAMercadoriaChega();
    void vendaSemEstoqueComCustoAnteriorUsaOCustoDaCompra();
    void compraParcialCobreSoParteDaFalta();
    void vendaCanceladaNaoRecebeCusto();

private:
    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_db;
    int m_usuarioId = 0;
    int m_sessaoId = 0;
    int m_prod = 0;
    int m_emb = 0;

    qint64 custoMedio();   // milésimos de centavo por unidade
    bool vender(int qtd, qint64 preco, int *vendaId = nullptr);
    qint64 lucroHoje() { return RelatorioRepository(m_db->connection()).faturamento(0).lucro; }
    qint64 custoHoje() { return RelatorioRepository(m_db->connection()).faturamento(0).custo; }
    qint64 escalar(const QString &sql)
    {
        QSqlQuery q(m_db->connection());
        return (q.exec(sql) && q.next()) ? q.value(0).toLongLong() : Q_INT64_C(-999999);
    }
};

void TstCustoEstoqueNegativo::init()
{
    m_dir.reset(new QTemporaryDir);
    m_db.reset(new Database);
    QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db->lastError()));
    MigrationRunner runner(m_db->connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery u(m_db->connection());
    QVERIFY(u.exec(QStringLiteral(
        "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) VALUES (1,'A','a','x',1)")));
    m_usuarioId = u.lastInsertId().toInt();

    ProdutoRepository prepo(m_db->connection());
    Produto p;
    p.nome = QStringLiteral("Lata de energetico");
    Embalagem e;
    e.nome = QStringLiteral("Unidade");
    e.fator = 1;
    e.precoVenda = 500;   // R$ 5,00
    p.embalagens = {e};
    QVERIFY(prepo.salvar(p));
    m_prod = p.id;
    m_emb = p.embalagens.first().id;

    CaixaRepository crepo(m_db->connection());
    m_sessaoId = crepo.abrirSessao(0, m_usuarioId);
    QVERIFY(m_sessaoId > 0);
}

void TstCustoEstoqueNegativo::cleanup()
{
    m_db.reset();
    m_dir.reset();
}

qint64 TstCustoEstoqueNegativo::custoMedio()
{
    QSqlQuery q(m_db->connection());
    q.prepare(QStringLiteral("SELECT custo_medio_unitario FROM estoque WHERE produto_id = :p"));
    q.bindValue(QStringLiteral(":p"), m_prod);
    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

bool TstCustoEstoqueNegativo::vender(int qtd, qint64 preco, int *vendaId)
{
    VendaRepository vrepo(m_db->connection());
    LinhaVenda l;
    l.produtoId = m_prod;
    l.embalagemId = m_emb;
    l.fator = 1;
    l.qtdEmbalagem = qtd;
    l.precoUnit = preco;
    PagamentoVenda pg;
    pg.forma = QStringLiteral("dinheiro");
    pg.valor = qtd * preco;
    const ResultadoVenda r = vrepo.registrarVenda(m_sessaoId, 0, 0, {l}, {pg}, m_usuarioId);
    if (!r.ok)
        qWarning("venda recusada: %s", qPrintable(r.erro));
    if (vendaId)
        *vendaId = r.vendaId;
    return r.ok;
}

// O caso da loja: vendeu antes de lançar, depois comprou.
void TstCustoEstoqueNegativo::compraDepoisDeVenderSemEstoque()
{
    // 20 latas vendidas sem nada no estoque.
    QVERIFY(vender(20, 500));

    // Chega a compra: 24 latas a R$ 3,00 cada.
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 24, 300, m_usuarioId, QString()));

    // O custo de cada lata é R$ 3,00 — e não R$ 18,00.
    QCOMPARE(custoMedio(), Q_INT64_C(300000));

    // E a próxima venda dá o lucro que dá na vida real: 5,00 − 3,00.
    const qint64 lucroAntes = RelatorioRepository(m_db->connection()).faturamento(0).lucro;
    QVERIFY(vender(1, 500));
    const qint64 lucroDepois = RelatorioRepository(m_db->connection()).faturamento(0).lucro;
    QCOMPARE(lucroDepois - lucroAntes, Q_INT64_C(200));
}

// Entrada sem custo informado não pode zerar nem inventar custo, com saldo
// negativo ou não: mantém o que havia.
void TstCustoEstoqueNegativo::entradaSemCustoComSaldoNegativoNaoZera()
{
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 300, m_usuarioId, QString()));
    QVERIFY(vender(15, 500));   // saldo −5, custo 3,00

    QVERIFY(erepo.registrarEntradaMilli(m_prod, 8, -1, m_usuarioId, QString()));   // sem custo
    QCOMPARE(custoMedio(), Q_INT64_C(300000));
}

// Com saldo positivo, nada muda: continua sendo média ponderada.
void TstCustoEstoqueNegativo::mediaNormalContinuaPonderada()
{
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 300, m_usuarioId, QString()));   // 10 × 3,00
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 500, m_usuarioId, QString()));   // 10 × 5,00
    QCOMPARE(custoMedio(), Q_INT64_C(400000));                                   // média 4,00
}

// Estoque que entrou SEM custo (custo médio 0) não é mercadoria de graça: é
// custo desconhecido. A média com ele dava 1,50 onde o custo é 3,00.
void TstCustoEstoqueNegativo::estoqueSemCustoNaoEntraNaMedia()
{
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntradaMilli(m_prod, 10, -1, m_usuarioId, QString()));   // sem custo
    QCOMPARE(custoMedio(), Q_INT64_C(0));
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 300, m_usuarioId, QString()));       // 10 × 3,00
    QCOMPARE(custoMedio(), Q_INT64_C(300000));
}

// ---------------------------------------------------------------------------
// Venda de produto SEM ESTOQUE: o custo das unidades vendidas "a descoberto" só
// é conhecido quando a mercadoria chega. Antes, a venda gravava o custo médio do
// momento e ele ficava travado para sempre — e produto que nunca teve custo saía
// com custo ZERO, lucro de 100%, mesmo depois da compra lançada.
// ---------------------------------------------------------------------------

// O caso da loja: vendeu antes de lançar um produto que nunca tinha entrado.
void TstCustoEstoqueNegativo::vendaSemEstoqueGanhaCustoQuandoAMercadoriaChega()
{
    QVERIFY(vender(5, 500));                 // 25,00 de receita, sem estoque
    QCOMPARE(custoHoje(), Q_INT64_C(0));     // ainda não se sabe o custo

    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 24, 300, m_usuarioId, QString()));   // chega a 3,00

    // As 5 vendidas custaram 3,00 cada: lucro 25,00 − 15,00.
    QCOMPARE(custoHoje(), Q_INT64_C(1500));
    QCOMPARE(lucroHoje(), Q_INT64_C(1000));
}

// Tinha custo, vendeu além do que havia: as que existiam saem com o custo
// antigo, as que faltavam com o custo da compra que chegou.
void TstCustoEstoqueNegativo::vendaSemEstoqueComCustoAnteriorUsaOCustoDaCompra()
{
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 300, m_usuarioId, QString()));   // 10 a 3,00
    QVERIFY(vender(15, 500));                                                   // 5 a descoberto
    QVERIFY(erepo.registrarEntrada(m_prod, 20, 400, m_usuarioId, QString()));   // chega a 4,00

    // 10 × 3,00 + 5 × 4,00 = 50,00.
    QCOMPARE(custoHoje(), Q_INT64_C(5000));
    QCOMPARE(lucroHoje(), Q_INT64_C(7500 - 5000));
}

// A compra que chega pode não cobrir toda a falta: só as unidades cobertas
// ganham custo; o resto espera a próxima compra.
void TstCustoEstoqueNegativo::compraParcialCobreSoParteDaFalta()
{
    QVERIFY(vender(20, 500));                                                   // 20 a descoberto
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 5, 300, m_usuarioId, QString()));    // cobre 5 a 3,00
    QCOMPARE(custoHoje(), Q_INT64_C(1500));
    // A saída foi dividida em duas linhas: as 20 vendidas continuam 20, e ainda
    // faltam 15 sem custo.
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(-quantidade) FROM movimentacoes_estoque "
                                    "WHERE tipo='saida_venda'")), Q_INT64_C(20));
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(qtd_pendente_custo) FROM movimentacoes_estoque")),
             Q_INT64_C(15));
    // O saldo do estoque segue batendo com a soma das movimentações.
    QCOMPARE(escalar(QStringLiteral("SELECT quantidade_atual FROM estoque WHERE produto_id=%1").arg(m_prod)),
             escalar(QStringLiteral("SELECT SUM(quantidade) FROM movimentacoes_estoque WHERE produto_id=%1").arg(m_prod)));

    QVERIFY(erepo.registrarEntrada(m_prod, 30, 400, m_usuarioId, QString()));   // cobre as 15 a 4,00
    QCOMPARE(custoHoje(), Q_INT64_C(1500 + 6000));
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(qtd_pendente_custo) FROM movimentacoes_estoque")),
             Q_INT64_C(0));
    QCOMPARE(escalar(QStringLiteral("SELECT quantidade_atual FROM estoque WHERE produto_id=%1").arg(m_prod)),
             Q_INT64_C(15));   // −20 + 5 + 30
    QCOMPARE(escalar(QStringLiteral("SELECT SUM(quantidade) FROM movimentacoes_estoque WHERE produto_id=%1").arg(m_prod)),
             Q_INT64_C(15));

    // E a compra seguinte, com o estoque já positivo, não mexe em venda nenhuma.
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 900, m_usuarioId, QString()));
    QCOMPARE(custoHoje(), Q_INT64_C(7500));
}

// Venda cancelada não pode "roubar" a mercadoria que chega: o custo tem que ir
// para quem ainda está a descoberto de verdade.
void TstCustoEstoqueNegativo::vendaCanceladaNaoRecebeCusto()
{
    int cancelada = 0;
    QVERIFY(vender(5, 500, &cancelada));    // a descoberto, depois cancelada
    VendaRepository vrepo(m_db->connection());
    QVERIFY2(vrepo.cancelarVenda(cancelada, QStringLiteral("teste"), m_usuarioId, m_sessaoId),
             qPrintable(vrepo.ultimoErro()));

    QVERIFY(vender(3, 500));                // esta sim fica a descoberto
    EstoqueRepository erepo(m_db->connection());
    QVERIFY(erepo.registrarEntrada(m_prod, 10, 300, m_usuarioId, QString()));

    QCOMPARE(custoHoje(), Q_INT64_C(900));  // só as 3 da venda que valeu
    QSqlQuery q(m_db->connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COALESCE(SUM(qtd_pendente_custo),0) FROM movimentacoes_estoque")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toLongLong(), Q_INT64_C(0));
}

QTEST_MAIN(TstCustoEstoqueNegativo)
#include "tst_custo_estoque_negativo.moc"
