#include <QtTest>

#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"

class TstMigrations : public QObject
{
    Q_OBJECT

private slots:
    void aplicaMigrationsESeed();
    void migracaoEhIdempotente();
    void foreignKeysLigadas();

private:
    QTemporaryDir m_dir;
    QString caminho() const { return m_dir.filePath(QStringLiteral("teste.db")); }
};

void TstMigrations::aplicaMigrationsESeed()
{
    Database db;
    QVERIFY2(db.open(caminho()), qUtf8Printable(db.lastError()));

    MigrationRunner runner(db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    // Primeira execução deve ter aplicado ao menos a migration inicial.
    QVERIFY(!runner.appliedInLastRun().isEmpty());

    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError()));

    // Seed populou categorias e perfis.
    QSqlQuery q(db.connection());
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM categorias")));
    QVERIFY(q.next());
    QVERIFY(q.value(0).toInt() > 0);

    QVERIFY(q.exec(QStringLiteral(
        "SELECT COUNT(*) FROM perfis WHERE nome = 'Administrador'")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);

    // A migration ficou registrada.
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM schema_migrations")));
    QVERIFY(q.next());
    QVERIFY(q.value(0).toInt() >= 1);
}

void TstMigrations::migracaoEhIdempotente()
{
    Database db;
    QVERIFY2(db.open(caminho()), qUtf8Printable(db.lastError()));

    // O banco já foi migrado no teste anterior (mesmo arquivo). Migrar de novo
    // não deve aplicar nada.
    MigrationRunner runner(db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY(runner.appliedInLastRun().isEmpty());
}

void TstMigrations::foreignKeysLigadas()
{
    Database db;
    QVERIFY2(db.open(caminho()), qUtf8Printable(db.lastError()));

    QSqlQuery q(db.connection());
    QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

QTEST_MAIN(TstMigrations)
#include "tst_migrations.moc"
