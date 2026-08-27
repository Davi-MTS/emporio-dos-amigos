#include <QtTest>

#include <QFileInfo>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "services/backup/BackupService.h"

class TstBackupService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void backupCriaArquivoIntegro();
    void retencaoMantemCincoMaisRecentes();
    void restauracaoRoundTrip();

private:
    QTemporaryDir m_dbDir;
    QTemporaryDir m_bkpDir;
    Database m_db;

    static qint64 contarEm(const QString &arquivoDb, const QString &tabela);
};

qint64 TstBackupService::contarEm(const QString &arquivoDb, const QString &tabela)
{
    qint64 n = -1;
    const QString conn = QStringLiteral("bkpcheck");
    {
        QSqlDatabase b = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        b.setDatabaseName(arquivoDb);
        if (b.open()) {
            QSqlQuery q(b);
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tabela)) && q.next())
                n = q.value(0).toLongLong();
            b.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return n;
}

void TstBackupService::initTestCase()
{
    QVERIFY2(m_db.open(m_dbDir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    QSqlQuery q(m_db.connection());
    QVERIFY(q.exec(QStringLiteral("INSERT INTO produtos (nome) VALUES ('Produto A')")));
    QVERIFY(q.exec(QStringLiteral("INSERT INTO produtos (nome) VALUES ('Produto B')")));
    QVERIFY(q.exec(QStringLiteral(
        "INSERT INTO clientes (nome, ativo) VALUES ('Cliente X', 1)")));
}

void TstBackupService::backupCriaArquivoIntegro()
{
    BackupService bs(m_db.connection(), m_bkpDir.path());
    BackupInfo info;
    QVERIFY2(bs.criarBackup(&info), qUtf8Printable(bs.ultimoErro()));

    // Arquivo e sidecar criados.
    QVERIFY(QFileInfo::exists(info.caminho));
    QVERIFY(QFileInfo::exists(info.caminho + QStringLiteral(".json")));
    QVERIFY(info.tamanho > 0);

    // Contagens capturadas.
    QCOMPARE(info.contagens.value(QStringLiteral("produtos")).toLongLong(), qint64(2));
    QCOMPARE(info.contagens.value(QStringLiteral("clientes")).toLongLong(), qint64(1));

    // O backup abre e tem os mesmos dados (cópia íntegra).
    QCOMPARE(contarEm(info.caminho, QStringLiteral("produtos")), qint64(2));
    QCOMPARE(contarEm(info.caminho, QStringLiteral("clientes")), qint64(1));
}

void TstBackupService::retencaoMantemCincoMaisRecentes()
{
    // Pasta isolada para não misturar com os outros testes.
    const QString dir = m_bkpDir.filePath(QStringLiteral("ret"));
    BackupService bs(m_db.connection(), dir);

    for (int i = 0; i < 6; ++i)
        QVERIFY2(bs.criarBackup(nullptr), qUtf8Printable(bs.ultimoErro()));
    QVERIFY(bs.listarBackups().size() >= 6);

    QVERIFY(bs.rotacionar(5));
    QCOMPARE(bs.listarBackups().size(), 5);
}

void TstBackupService::restauracaoRoundTrip()
{
    const QString dbPath = m_db.connection().databaseName();

    BackupInfo estadoA;
    {
        BackupService bs(m_db.connection(), m_bkpDir.filePath(QStringLiteral("rt")));
        // Estado A: 2 produtos.
        QVERIFY2(bs.criarBackup(&estadoA), qUtf8Printable(bs.ultimoErro()));

        // Estado B: adiciona um produto (agora 3).
        QSqlQuery q(m_db.connection());
        QVERIFY(q.exec(QStringLiteral("INSERT INTO produtos (nome) VALUES ('Produto C')")));
        QCOMPARE(contarEm(dbPath, QStringLiteral("produtos")), qint64(3));

        // Agenda voltar para o estado A (cria backup de emergência + marcador).
        QVERIFY2(bs.agendarRestauracao(estadoA.caminho), qUtf8Printable(bs.ultimoErro()));
        QVERIFY(QFileInfo::exists(dbPath + QStringLiteral(".restore")));
    } // bs sai de escopo antes de fechar a conexão

    m_db.close();

    QString erro;
    QVERIFY2(BackupService::aplicarRestauracaoPendente(dbPath, &erro), qUtf8Printable(erro));
    // Marcador removido após aplicar.
    QVERIFY(!QFileInfo::exists(dbPath + QStringLiteral(".restore")));

    // Reabre e confirma que voltou ao estado A (2 produtos).
    QVERIFY2(m_db.open(dbPath), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError())); // no-op (mesmo schema)
    QCOMPARE(contarEm(dbPath, QStringLiteral("produtos")), qint64(2));
}

QTEST_MAIN(TstBackupService)
#include "tst_backup_service.moc"
