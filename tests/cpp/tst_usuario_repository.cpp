#include <QtTest>

#include <QTemporaryDir>

#include "database/Database.h"
#include "database/MigrationRunner.h"
#include "domain/usuarios/UsuarioRepository.h"

class TstUsuarioRepository : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void criaPrimeiroAdminEAutentica();
    void senhaErradaFalha();
    void criaFuncionarioComPerfil();
    void loginDuplicadoFalha();
    void inativarBloqueiaLogin();

private:
    QTemporaryDir m_dir;
    Database m_db;
    UsuarioRepository repo() { return UsuarioRepository(m_db.connection()); }
    int m_funcId = 0;
};

void TstUsuarioRepository::initTestCase()
{
    QVERIFY2(m_db.open(m_dir.filePath(QStringLiteral("t.db"))), qUtf8Printable(m_db.lastError()));
    MigrationRunner runner(m_db.connection());
    QVERIFY2(runner.migrate(), qUtf8Printable(runner.lastError()));
    QVERIFY2(runner.seed(), qUtf8Printable(runner.lastError())); // perfis 1 e 2
}

void TstUsuarioRepository::criaPrimeiroAdminEAutentica()
{
    auto r = repo();
    QCOMPARE(r.contarComSenha(), 0);
    QVERIFY2(r.criarPrimeiroAdmin(QStringLiteral("Chefe"), QStringLiteral("admin"), QStringLiteral("1234")),
             qUtf8Printable(r.ultimoErro()));
    QCOMPARE(r.contarComSenha(), 1);

    const auto u = r.autenticar(QStringLiteral("admin"), QStringLiteral("1234"));
    QVERIFY(u.has_value());
    QCOMPARE(u->nome, QStringLiteral("Chefe"));
    QCOMPARE(u->perfilNome, QStringLiteral("Administrador"));
    QVERIFY(u->permissoesJson.contains(QStringLiteral("tudo")));
}

void TstUsuarioRepository::senhaErradaFalha()
{
    auto r = repo();
    QVERIFY(!r.autenticar(QStringLiteral("admin"), QStringLiteral("errada")).has_value());
    QVERIFY(!r.autenticar(QStringLiteral("naoexiste"), QStringLiteral("1234")).has_value());
}

void TstUsuarioRepository::criaFuncionarioComPerfil()
{
    auto r = repo();
    Usuario f;
    f.nome = QStringLiteral("Fulano");
    f.login = QStringLiteral("fulano");
    f.perfilId = 2; // Funcionário
    QVERIFY2(r.salvar(f, QStringLiteral("abcd")), qUtf8Printable(r.ultimoErro()));
    QVERIFY(f.id > 0);
    m_funcId = f.id;

    const auto u = r.autenticar(QStringLiteral("fulano"), QStringLiteral("abcd"));
    QVERIFY(u.has_value());
    QCOMPARE(u->perfilNome, QStringLiteral("Funcionário"));
    QVERIFY(!u->permissoesJson.contains(QStringLiteral("\"tudo\"")));
}

void TstUsuarioRepository::loginDuplicadoFalha()
{
    auto r = repo();
    Usuario f;
    f.nome = QStringLiteral("Outro");
    f.login = QStringLiteral("fulano"); // já existe
    f.perfilId = 2;
    QVERIFY(!r.salvar(f, QStringLiteral("abcd")));
    QVERIFY(!r.ultimoErro().isEmpty());
}

void TstUsuarioRepository::inativarBloqueiaLogin()
{
    auto r = repo();
    QVERIFY(r.inativar(m_funcId));
    QVERIFY(!r.autenticar(QStringLiteral("fulano"), QStringLiteral("abcd")).has_value());
}

QTEST_MAIN(TstUsuarioRepository)
#include "tst_usuario_repository.moc"
