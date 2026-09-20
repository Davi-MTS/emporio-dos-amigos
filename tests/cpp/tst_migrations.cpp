#include <QtTest>

#include <QSqlQuery>
#include <QSqlError>
#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/clientes/ClienteRepository.h"
#include "domain/produtos/ProdutoRepository.h"

class TstMigrations : public QObject
{
    Q_OBJECT

private slots:
    void aplicaMigrationsESeed();
    void migracaoEhIdempotente();
    void foreignKeysLigadas();
    void criadoEmConvertidoParaHoraLocal();
    void cadastroNovoGravaHoraLocal();

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

// 0018: o que já estava gravado em UTC passa para hora local. Simula um banco
// de antes da 0018 (linha em UTC, 0018 ainda não aplicada) e migra.
void TstMigrations::criadoEmConvertidoParaHoraLocal()
{
    Database db;
    QVERIFY2(db.open(caminho()), qUtf8Printable(db.lastError()));
    QSqlQuery q(db.connection());
    QVERIFY(q.exec(QStringLiteral(
        "INSERT INTO clientes (nome, criado_em) VALUES ('Cliente UTC', '2026-09-10 01:30:00')")));
    QVERIFY(q.exec(QStringLiteral(
        "DELETE FROM schema_migrations WHERE version = '0018_criado_em_local.sql'")));
    QCOMPARE(q.numRowsAffected(), 1);

    MigrationRunner runner(db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QCOMPARE(runner.appliedInLastRun(), QStringList{QStringLiteral("0018_criado_em_local.sql")});

    QVERIFY(q.exec(QStringLiteral(
        "SELECT criado_em = datetime('2026-09-10 01:30:00', 'localtime') "
        "FROM clientes WHERE nome = 'Cliente UTC'")));
    QVERIFY(q.next());
    QVERIFY2(q.value(0).toBool(), "criado_em não foi convertido de UTC para hora local");
}

// Daqui em diante, cadastro novo já nasce em hora local (e não no DEFAULT UTC).
void TstMigrations::cadastroNovoGravaHoraLocal()
{
    Database db;
    QVERIFY2(db.open(caminho()), qUtf8Printable(db.lastError()));

    Produto p;
    p.nome = QStringLiteral("Produto Hora Local");
    Embalagem e;
    e.nome = QStringLiteral("Unidade");
    p.embalagens.push_back(e);
    ProdutoRepository prod(db.connection());
    QVERIFY2(prod.salvar(p), qUtf8Printable(prod.ultimoErro()));

    Cliente c;
    c.nome = QStringLiteral("Cliente Hora Local");
    ClienteRepository cli(db.connection());
    QVERIFY2(cli.salvar(c), qUtf8Printable(cli.ultimoErro()));

    // Tolerância de 2 min para o relógio andar durante o teste; a diferença
    // de fuso (UTC−3) é de horas, então não passa por acaso.
    QSqlQuery q(db.connection());
    for (const QString &sql : {
             QStringLiteral("SELECT criado_em FROM produtos WHERE nome = 'Produto Hora Local'"),
             QStringLiteral("SELECT criado_em FROM clientes WHERE nome = 'Cliente Hora Local'")}) {
        QVERIFY2(q.exec(sql), qUtf8Printable(q.lastError().text()));
        QVERIFY(q.next());
        const QDateTime gravado = QDateTime::fromString(q.value(0).toString(),
                                                        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        QVERIFY2(gravado.isValid(), qUtf8Printable(q.value(0).toString()));
        const qint64 diff = qAbs(gravado.secsTo(QDateTime::currentDateTime()));
        QVERIFY2(diff < 120, qUtf8Printable(QStringLiteral("%1 está %2 s longe da hora local")
                                                .arg(q.value(0).toString()).arg(diff)));
    }
}

QTEST_MAIN(TstMigrations)
#include "tst_migrations.moc"
