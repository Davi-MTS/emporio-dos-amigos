#pragma once

#include <QString>
#include <QtGlobal>
#include <optional>

// Valores monetários vivem SEMPRE como qint64 em centavos no sistema inteiro.
// Estas funções apenas formatam para exibição e fazem parse de entrada do
// usuário. Nunca use double para dinheiro (ver CLAUDE.md: fechamento de caixa
// não pode ter erro silencioso).
namespace Money {

// 1250 -> "R$ 12,50"
QString format(qint64 centavos);

// 1250 -> "12,50" (sem símbolo, para campos de edição)
QString formatPlain(qint64 centavos);

// Faz parse de "12,50", "12.50", "R$ 12,50" ou "1250,00" para centavos.
// Retorna nullopt se a entrada for inválida.
std::optional<qint64> parse(const QString &texto);

} // namespace Money
