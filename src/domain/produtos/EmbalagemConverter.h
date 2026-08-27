#pragma once

#include <QtGlobal>

// Conversão de embalagem — CONCEITO CENTRAL do sistema (ver CLAUDE.md).
//
// O estoque guarda SEMPRE um único inteiro, na unidade base do produto.
// Cada embalagem tem um `fator_conversao`: quantas unidades base ela representa
// (ex.: caixa de Heineken = fator 12). Vender/comprar em caixa ou unidade só
// multiplica/divide por esse fator.
//
// Funções puras, sem dependência de banco ou UI — cobertas por testes em
// tests/cpp/. Esta é uma das partes que NÃO podem ter erro silencioso.
namespace EmbalagemConverter {

// Quantas unidades base equivalem a `qtdEmbalagem` embalagens de fator `fator`.
// Ex.: 3 caixas de fator 12 -> 36 unidades base.
// Pré-condição: fator > 0. Se fator <= 0, retorna 0.
qint64 paraUnidadeBase(qint64 qtdEmbalagem, int fator);

// Resultado de "quantas embalagens fechadas cabem em X unidades base".
struct Desmembramento {
    qint64 embalagensFechadas; // parte inteira (qtdBase / fator)
    qint64 sobra;              // resto em unidade base (qtdBase % fator)
};

// Ex.: 37 unidades base com fator 12 -> { 3 caixas fechadas, sobra 1 }.
// Pré-condição: fator > 0. Se fator <= 0, retorna { 0, qtdBase }.
Desmembramento desmembrar(qint64 qtdBase, int fator);

// Deriva um valor (custo/preço) no nível da embalagem a partir do valor por
// unidade base. Ex.: custo base 250 centavos, fator 12 -> caixa custa 3000.
// Pré-condição: fator > 0. Se fator <= 0, retorna 0.
qint64 valorDaEmbalagem(qint64 valorPorUnidadeBase, int fator);

} // namespace EmbalagemConverter
