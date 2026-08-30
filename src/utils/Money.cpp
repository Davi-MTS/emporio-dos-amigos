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
    // Descarta o ruído LEGÍTIMO — o "R$" e os espaços — e mais nada.
    //
    // Antes, qualquer caractere fora do conjunto era simplesmente jogado fora.
    // Isso parecia tolerante e era perigoso: "1OO" digitado com a letra O (erro
    // de quem digita rápido olhando o teclado) virava "1", ou seja, R$ 1,00 no
    // lugar de R$ 100,00 — sem um aviso sequer. Em abertura de caixa e em
    // contagem de gaveta, isso vira diferença que ninguém consegue explicar.
    QString limpo = texto.trimmed();
    // Sem regex de proposito: escapar \$ em literal C++ e fonte de erro bobo.
    if (limpo.startsWith(QLatin1String("R$"), Qt::CaseInsensitive))
        limpo.remove(0, 2);
    limpo.remove(QLatin1Char(' '));
    limpo.remove(QChar(0x00A0));   // espaço não separável (vem de copiar/colar)

    if (limpo.isEmpty() || limpo == QLatin1String("-"))
        return std::nullopt;

    // Daqui em diante, QUALQUER caractere estranho invalida a entrada inteira.
    for (int i = 0; i < limpo.size(); ++i) {
        const QChar c = limpo.at(i);
        const bool aceito = c.isDigit() || c == QLatin1Char(',') || c == QLatin1Char('.')
                            || (c == QLatin1Char('-') && i == 0);
        if (!aceito)
            return std::nullopt;
    }

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
