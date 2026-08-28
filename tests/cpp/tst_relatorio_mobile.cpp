#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "services/relatoriomobile/RelatorioMobileService.h"

class TstRelatorioMobile : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void geraHtmlComDados();
    void aguentaEstoqueGrande();

private:
    QTemporaryDir m_dbDir;
    QTemporaryDir m_outDir;
    Database m_db;
};

void TstRelatorioMobile::initTestCase()
{
    QVERIFY2(m_db.open(m_dbDir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("INSERT INTO produtos (nome) VALUES ('ProdutoTesteXYZ')")));
}

void TstRelatorioMobile::geraHtmlComDados()
{
    RelatorioMobileService svc(m_db.connection(), m_outDir.path());
    QString caminho;
    QVERIFY2(svc.gerar(&caminho), qUtf8Printable(svc.ultimoErro()));

    // Arquivo criado, não vazio.
    QVERIFY(QFileInfo::exists(caminho));
    QVERIFY(QFileInfo(caminho).size() > 0);

    // Conteúdo esperado: marca + produto (na seção de estoque) + JSON embutido.
    QFile f(caminho);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(f.readAll());
    f.close();

    QVERIFY(html.contains(QStringLiteral("Empório dos Amigos")));
    QVERIFY(html.contains(QStringLiteral("ProdutoTesteXYZ")));
    QVERIFY(html.contains(QStringLiteral("id=\"dados\"")));  // JSON embutido
    QVERIFY(html.contains(QStringLiteral("periodos")));

    // Relatório COMPLETO: todas as áreas do negócio presentes no JSON.
    for (const char *chave : {"estoque", "estoqueValor", "fiado", "aPagar",
                              "compras", "parados", "caixa"}) {
        QVERIFY2(html.contains(QStringLiteral("\"%1\"").arg(QLatin1String(chave))),
                 qUtf8Printable(QStringLiteral("faltou a seção '%1' no relatório")
                                    .arg(QLatin1String(chave))));
    }
    // E as seções renderizadas na página.
    QVERIFY(html.contains(QStringLiteral("Último fechamento de caixa")));
    QVERIFY(html.contains(QStringLiteral("A pagar")));
    QVERIFY(html.contains(QStringLiteral("Últimas compras")));
}

void TstRelatorioMobile::aguentaEstoqueGrande()
{
    // Uma distribuidora real tem centenas de itens. O relatório precisa
    // continuar leve e navegável — não virar uma parede de rolagem.
    QSqlQuery q(m_db.connection());
    for (int i = 0; i < 200; ++i) {
        q.prepare(QStringLiteral(
            "INSERT INTO produtos (nome, estoque_minimo) VALUES (:n, 10)"));
        q.bindValue(QStringLiteral(":n"), QStringLiteral("Produto %1").arg(i, 3, 10, QLatin1Char('0')));
        QVERIFY(q.exec());
        const int pid = q.lastInsertId().toInt();
        QSqlQuery e(m_db.connection());
        e.prepare(QStringLiteral(
            "INSERT INTO estoque (produto_id, quantidade_atual, custo_medio_unitario) "
            "VALUES (:p, :q, 1500)"));
        e.bindValue(QStringLiteral(":p"), pid);
        e.bindValue(QStringLiteral(":q"), (i % 10 == 0) ? 0 : 50);  // 10% em falta
        QVERIFY(e.exec());
    }

    RelatorioMobileService svc(m_db.connection(), m_outDir.path());
    QString caminho;
    QVERIFY2(svc.gerar(&caminho), qUtf8Printable(svc.ultimoErro()));

    QFile f(caminho);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(f.readAll());
    f.close();

    // Continua um arquivo pequeno, que abre rápido no celular.
    QVERIFY2(html.size() < 400000,
             qUtf8Printable(QStringLiteral("relatório grande demais: %1 bytes").arg(html.size())));

    // As seções pesadas vêm recolhidas e o estoque tem filtro/paginação.
    QVERIFY(html.contains(QStringLiteral("<details>")));       // fechada por padrão
    QVERIFY(html.contains(QStringLiteral("chipFalta")));       // filtro "só os que faltam"
    QVERIFY(html.contains(QStringLiteral("verMais")));         // paginação por lotes
    QVERIFY(html.contains(QStringLiteral("buscaEstoque")));    // busca

    // O resumo do Telegram não pode inchar com 200 produtos.
    const QString msg = svc.resumoTexto();
    QVERIFY2(msg.size() < 4096,
             qUtf8Printable(QStringLiteral("mensagem grande demais: %1").arg(msg.size())));
    QVERIFY(msg.contains(QStringLiteral("no estoque mínimo")));
}

QTEST_MAIN(TstRelatorioMobile)
#include "tst_relatorio_mobile.moc"
