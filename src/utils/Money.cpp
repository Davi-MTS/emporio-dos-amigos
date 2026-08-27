#include "utils/Money.h"

#include <QChar>

namespace Money {

QString formatPlain(qint64 centavos)
{
    const bool negativo = centavos < 0;
    const qint64 abs = negativo ? -centavos : centavos;
    const qint64 reais = abs / 100;
    const qint64 cents = abs % 100;

    // Milhar com ponto (pt-BR): 1234567 centavos -> "12.345,67"
    QString inteiro = QString::number(reais);
    QString comMilhar;
    int contador = 0;
    for (int i = inteiro.size() - 1; i >= 0; --i) {
        comMilhar.prepend(inteiro.at(i));
        if (++contador % 3 == 0 && i > 0)
            comMilhar.prepend(QLatin1Char('.'));
    }

    QString resultado = QStringLiteral("%1,%2")
                            .arg(comMilhar)
                            .arg(cents, 2, 10, QLatin1Char('0'));
    if (negativo)
        resultado.prepend(QLatin1Char('-'));
    return resultado;
}

QString format(qint64 centavos)
{
    return QStringLiteral("R$ ") + formatPlain(centavos);
}

std::optional<qint64> parse(const QString &texto)
{
    // Mantém apenas dígitos e separadores; descarta "R$", espaços etc.
    QString limpo;
    for (const QChar c : texto) {
        if (c.isDigit() || c == QLatin1Char(',') || c == QLatin1Char('.')
            || c == QLatin1Char('-')) {
            limpo.append(c);
        }
    }
    if (limpo.isEmpty() || limpo == QLatin1String("-"))
        return std::nullopt;

    const bool negativo = limpo.startsWith(QLatin1Char('-'));
    if (negativo)
        limpo.remove(0, 1);

    // Descobre o separador decimal: o ÚLTIMO ',' ou '.' presente.
    const int posVirgula = limpo.lastIndexOf(QLatin1Char(','));
    const int posPonto = limpo.lastIndexOf(QLatin1Char('.'));
    const int posDecimal = qMax(posVirgula, posPonto);

    QString parteInteira;
    QString parteDecimal;
    if (posDecimal >= 0) {
        parteInteira = limpo.left(posDecimal);
        parteDecimal = limpo.mid(posDecimal + 1);
    } else {
        parteInteira = limpo;
    }

    // Remove separadores de milhar restantes da parte inteira.
    parteInteira.remove(QLatin1Char(',')).remove(QLatin1Char('.'));

    // A parte decimal não pode conter mais separadores; se contiver, é inválido.
    if (parteDecimal.contains(QLatin1Char(',')) || parteDecimal.contains(QLatin1Char('.')))
        return std::nullopt;

    // Normaliza a parte decimal para exatamente 2 dígitos (centavos).
    if (parteDecimal.size() > 2)
        return std::nullopt; // mais de 2 casas: ambíguo, rejeita
    while (parteDecimal.size() < 2)
        parteDecimal.append(QLatin1Char('0'));

    if (parteInteira.isEmpty())
        parteInteira = QStringLiteral("0");

    bool okInt = false;
    bool okDec = false;
    const qint64 reais = parteInteira.toLongLong(&okInt);
    const qint64 cents = parteDecimal.toLongLong(&okDec);
    if (!okInt || !okDec)
        return std::nullopt;

    const qint64 total = reais * 100 + cents;
    return negativo ? -total : total;
}

} // namespace Money
