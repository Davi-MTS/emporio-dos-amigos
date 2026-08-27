#pragma once

#include <QString>

// Hash de senha com PBKDF2-HMAC-SHA256 (sal aleatório por senha).
// Formato armazenado: "pbkdf2_sha256$<iteracoes>$<salHex>$<hashHex>".
// Nunca guardar senha em texto puro.
namespace AuthService {

QString hashSenha(const QString &senha);
bool verificar(const QString &senha, const QString &hashArmazenado);

} // namespace AuthService
