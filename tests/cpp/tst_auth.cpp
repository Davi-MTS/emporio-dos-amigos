#include <QtTest>

#include "services/auth/AuthService.h"

class TstAuth : public QObject
{
    Q_OBJECT

private slots:
    void hashVerificaCorreta();
    void rejeitaSenhaErrada();
    void salAleatorioGeraHashesDiferentes();
    void rejeitaHashMalformado();
};

void TstAuth::hashVerificaCorreta()
{
    const QString h = AuthService::hashSenha(QStringLiteral("segredo123"));
    QVERIFY(h.startsWith(QStringLiteral("pbkdf2_sha256$")));
    QVERIFY(AuthService::verificar(QStringLiteral("segredo123"), h));
}

void TstAuth::rejeitaSenhaErrada()
{
    const QString h = AuthService::hashSenha(QStringLiteral("segredo123"));
    QVERIFY(!AuthService::verificar(QStringLiteral("segredo124"), h));
    QVERIFY(!AuthService::verificar(QString(), h));
}

void TstAuth::salAleatorioGeraHashesDiferentes()
{
    const QString a = AuthService::hashSenha(QStringLiteral("mesma"));
    const QString b = AuthService::hashSenha(QStringLiteral("mesma"));
    QVERIFY(a != b);                                   // sais diferentes
    QVERIFY(AuthService::verificar(QStringLiteral("mesma"), a));
    QVERIFY(AuthService::verificar(QStringLiteral("mesma"), b));
}

void TstAuth::rejeitaHashMalformado()
{
    QVERIFY(!AuthService::verificar(QStringLiteral("x"), QStringLiteral("")));
    QVERIFY(!AuthService::verificar(QStringLiteral("x"), QStringLiteral("abc")));
    QVERIFY(!AuthService::verificar(QStringLiteral("x"), QStringLiteral("md5$1$aa$bb")));
}

QTEST_APPLESS_MAIN(TstAuth)
#include "tst_auth.moc"
