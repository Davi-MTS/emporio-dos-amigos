#pragma once

#include <QString>
#include <QtGlobal>

// Item de uma compra (entrada de mercadoria).
struct ItemCompra
{
    int produtoId = 0;
    int embalagemId = 0;
    int fator = 1;
    qint64 qtdEmbalagem = 0;         // quantas embalagens
    qint64 custoUnitEmbalagem = 0;   // centavos por embalagem
    QString validade;                // ISO date; vazio = produto que não vence
    QString codigoLote;              // opcional, o que vem impresso na caixa
};

struct ResultadoCompra
{
    bool ok = false;
    int compraId = 0;
    qint64 total = 0;                // centavos
    QString erro;
};

// Linha de listagem de compras.
struct CompraResumo
{
    int id = 0;
    QString fornecedorNome;
    QString data;
    qint64 total = 0;
    QString status;
    int numItens = 0;
    QString numeroNota;   // nº da NF de origem (vazio se lançamento manual)
};
