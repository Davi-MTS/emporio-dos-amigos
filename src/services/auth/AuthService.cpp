#include "services/auth/AuthService.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QPasswordDigestor>   // módulo QtNetwork
#include <QRandomGenerator>
#include <QStringList>

namespace {
constexpr int kIteracoes = 100000;
constexpr int kDkLen = 32;
constexpr int kSalLen = 16;
}

namespace AuthService {

QString hashSenha(const QString &senha)
{
    QByteArray sal(kSalLen, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(
        reinterpret_cast<quint32 *>(sal.data()), kSalLen / 4);

    const QByteArray dk = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, senha.toUtf8(), sal, kIteracoes, kDkLen);

    return QStringLiteral("pbkdf2_sha256$%1$%2$%3")
        .arg(kIteracoes)
        .arg(QString::fromLatin1(sal.toHex()),
             QString::fromLatin1(dk.toHex()));
}

bool verificar(const QString &senha, const QString &hashArmazenado)
{
    const QStringList partes = hashArmazenado.split(QLatin1Char('$'));
    if (partes.size() != 4 || partes.at(0) != QLatin1String("pbkdf2_sha256"))
        return false;

    bool okIter = false;
    const int iteracoes = partes.at(1).toInt(&okIter);
    if (!okIter || iteracoes <= 0)
        return false;

    const QByteArray sal = QByteArray::fromHex(partes.at(2).toLatin1());
    const QByteArray esperado = QByteArray::fromHex(partes.at(3).toLatin1());
    if (esperado.isEmpty())
        return false;

    const QByteArray dk = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, senha.toUtf8(), sal, iteracoes, esperado.size());

    // Comparação de tempo ~constante.
    if (dk.size() != esperado.size())
        return false;
    quint8 diff = 0;
    for (int i = 0; i < dk.size(); ++i)
        diff |= static_cast<quint8>(dk.at(i) ^ esperado.at(i));
    return diff == 0;
}

} // namespace AuthService
