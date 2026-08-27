#include "domain/produtos/EmbalagemConverter.h"

namespace EmbalagemConverter {

qint64 paraUnidadeBase(qint64 qtdEmbalagem, int fator)
{
    if (fator <= 0)
        return 0;
    return qtdEmbalagem * static_cast<qint64>(fator);
}

Desmembramento desmembrar(qint64 qtdBase, int fator)
{
    if (fator <= 0)
        return {0, qtdBase};
    const qint64 f = static_cast<qint64>(fator);
    return {qtdBase / f, qtdBase % f};
}

qint64 valorDaEmbalagem(qint64 valorPorUnidadeBase, int fator)
{
    if (fator <= 0)
        return 0;
    return valorPorUnidadeBase * static_cast<qint64>(fator);
}

} // namespace EmbalagemConverter
