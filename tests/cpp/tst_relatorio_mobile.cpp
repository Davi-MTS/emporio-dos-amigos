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
}

QTEST_MAIN(TstRelatorioMobile)
#include "tst_relatorio_mobile.moc"
