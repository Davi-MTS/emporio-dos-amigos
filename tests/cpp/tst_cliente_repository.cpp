#include <QtTest>

#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/caixa/CaixaRepository.h"
#include "domain/clientes/ClienteRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/produtos/ProdutoRepository.h"
#include "domain/vendas/VendaRepository.h"

class TstClienteRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void crudEsaldo();
    void fiadoDentroDoLimite();
    void fiadoExcedeLimite();
    void clienteSemLimiteRecusaFiado();
    void quitarZeraSaldo();
    void recebimentoParcialFifo();

private:
    QTemporaryDir m_dir;
    Database m_db;
    int m_usuarioId = 0, m_sessaoId = 0, m_produtoId = 0, m_embBaseId = 0;
    int m_cli1 = 0, m_cli2 = 0;

    ClienteRepository cli() { return ClienteRepository(m_db.connection()); }
    VendaRepository vendas() { return VendaRepository(m_db.connection()); }

    ResultadoVenda vendaFiado(int clienteId, qint64 valor, qint64 qtd)
    {
        QVector<LinhaVenda> itens;
        LinhaVenda l; l.produtoId = m_produtoId; l.embalagemId = m_embBaseId; l.fator = 1;
        l.qtdEmbalagem = qtd; l.precoUnit = 500; itens.push_back(l);
        QVector<PagamentoVenda> pags;
        PagamentoVenda p; p.forma = QStringLiteral("fiado"); p.valor = valor; pags.push_back(p);
        return vendas().registrarVenda(m_sessaoId, clienteId, 0, itens, pags, m_usuarioId);
    }
};

void TstClienteRepository::initTestCase()
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
    p.nome = QStringLiteral("Prod");
    Embalagem base; base.nome = QStringLiteral("Unidade"); base.fator = 1; base.precoVenda = 500;
    p.embalagens = {base};
    QVERIFY(prepo.salvar(p));
    m_produtoId = p.id;
    m_embBaseId = p.embalagens.first().id;

    EstoqueRepository erepo(m_db.connection());
    QVERIFY(erepo.registrarEntrada(m_produtoId, 100, 300, m_usuarioId, QString()));

    CaixaRepository crepo(m_db.connection());
    m_sessaoId = crepo.abrirSessao(0, m_usuarioId);
    QVERIFY(m_sessaoId > 0);

    ClienteRepository r = cli();
    Cliente c1; c1.nome = QStringLiteral("Cliente Fiado"); c1.limiteFiado = 5000;
    QVERIFY(r.salvar(c1)); m_cli1 = c1.id;
    Cliente c2; c2.nome = QStringLiteral("Sem Limite"); c2.limiteFiado = 0;
    QVERIFY(r.salvar(c2)); m_cli2 = c2.id;
}

void TstClienteRepository::crudEsaldo()
{
    ClienteRepository r = cli();
    const auto c = r.obter(m_cli1);
    QVERIFY(c.has_value());
    QCOMPARE(c->limiteFiado, qint64(5000));
    QCOMPARE(r.saldoDevedor(m_cli1), qint64(0));
    QCOMPARE(r.listar().size(), 2);
}

void TstClienteRepository::fiadoDentroDoLimite()
{
    const ResultadoVenda r = vendaFiado(m_cli1, 500, 1); // 1 un = 5,00
    QVERIFY2(r.ok, qUtf8Printable(r.erro));
    QCOMPARE(cli().saldoDevedor(m_cli1), qint64(500));
}

void TstClienteRepository::fiadoExcedeLimite()
{
    // Saldo 500 + 5000 = 5500 > 5000 -> recusa.
    const ResultadoVenda r = vendaFiado(m_cli1, 5000, 10); // 10 un = 50,00
    QVERIFY(!r.ok);
    QVERIFY(r.erro.contains(QStringLiteral("Limite")));
    QCOMPARE(cli().saldoDevedor(m_cli1), qint64(500)); // não mudou
}

void TstClienteRepository::clienteSemLimiteRecusaFiado()
{
    const ResultadoVenda r = vendaFiado(m_cli2, 500, 1);
    QVERIFY(!r.ok);
    QVERIFY(r.erro.contains(QStringLiteral("sem limite")));
}

void TstClienteRepository::quitarZeraSaldo()
{
    ClienteRepository r = cli();
    const qint64 quitado = r.quitar(m_cli1);
    QCOMPARE(quitado, qint64(500));
    QCOMPARE(r.saldoDevedor(m_cli1), qint64(0));
}

void TstClienteRepository::recebimentoParcialFifo()
{
    ClienteRepository r = cli();
    // Duas dívidas: 10,00 (mais antiga) e 5,00 -> saldo 15,00.
    QVERIFY2(vendaFiado(m_cli1, 1000, 2).ok, "venda fiado 1");
    QVERIFY2(vendaFiado(m_cli1, 500, 1).ok, "venda fiado 2");
    QCOMPARE(r.saldoDevedor(m_cli1), qint64(1500));

    // Recebe 12,00: quita a de 10,00 (FIFO) e abate 2,00 da de 5,00.
    const qint64 ap1 = r.aplicarRecebimento(m_cli1, 1200);
    QCOMPARE(ap1, qint64(1200));
    QCOMPARE(r.saldoDevedor(m_cli1), qint64(300));

    // Recebe além do saldo: aplica só o que resta (3,00), sem crédito.
    const qint64 ap2 = r.aplicarRecebimento(m_cli1, 5000);
    QCOMPARE(ap2, qint64(300));
    QCOMPARE(r.saldoDevedor(m_cli1), qint64(0));
}

QTEST_MAIN(TstClienteRepository)
#include "tst_cliente_repository.moc"
