#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QPair>
#include <QtGlobal>

// Insumo já resolvido de um produto composto (o produto específico escolhido na
// venda + a quantidade da receita por 1 unidade do composto).
struct InsumoResolvido
{
    int produtoId = 0;
    qint64 quantidade = 0;   // por 1 unidade do composto
};

// Uma linha do carrinho: um produto vendido numa embalagem, com quantidade
// (em número de embalagens) e o preço da embalagem.
struct LinhaVenda
{
    int produtoId = 0;
    int embalagemId = 0;
    int fator = 1;              // unidades base por embalagem
    qint64 qtdEmbalagem = 0;    // quantas embalagens
    qint64 precoUnit = 0;       // centavos por embalagem
    qint64 desconto = 0;        // centavos, nesta linha

    // Só para produtos compostos: os insumos escolhidos na venda.
    QVector<InsumoResolvido> insumos;
};

struct PagamentoVenda
{
    QString forma;             // pix | dinheiro | debito | credito | fiado
    qint64 valor = 0;          // centavos
};

struct ResultadoVenda
{
    bool ok = false;
    int vendaId = 0;
    qint64 total = 0;          // centavos
    qint64 troco = 0;          // centavos (excedente pago em dinheiro)
    QString erro;
};

// Registra vendas: grava a venda, os itens e pagamentos, baixa o estoque em
// unidade base e registra as movimentações. Tudo em transação.
class VendaRepository
{
public:
    explicit VendaRepository(QSqlDatabase db);

    ResultadoVenda registrarVenda(int sessaoId, int clienteId, qint64 descontoGeral,
                                  const QVector<LinhaVenda> &itens,
                                  const QVector<PagamentoVenda> &pagamentos,
                                  int usuarioId);

private:
    QSqlDatabase m_db;
};
