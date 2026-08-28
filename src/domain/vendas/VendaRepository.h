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

// Linha do histórico de vendas.
struct VendaResumo
{
    int id = 0;
    QString data;
    QString clienteNome;
    QString status;            // concluida | cancelada | troca
    QString formas;            // "Dinheiro, Pix"
    qint64 total = 0;
    qint64 troco = 0;
    int numItens = 0;
    QString motivoCancelamento;
};

// Um item de uma venda (para conferir o que foi vendido).
struct ItemVendido
{
    QString produto;
    QString embalagem;
    qint64 qtdBase = 0;
    qint64 precoUnit = 0;
    qint64 desconto = 0;
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

    // Cancela uma venda concluída: devolve os itens ao estoque (movimentação
    // 'devolucao'), cancela a conta de fiado gerada e marca a venda como
    // 'cancelada'. NUNCA apaga registros — o histórico continua auditável.
    // Se a venda for de um turno JÁ FECHADO e houver caixa aberto agora, o
    // dinheiro devolvido sai como sangria; senão o fechamento de hoje não bate.
    // Tudo numa transação. false com o motivo em ultimoErro().
    bool cancelarVenda(int vendaId, const QString &motivo, int usuarioId,
                       int sessaoAbertaId = 0);

    // Histórico (mais recentes primeiro). dias <= 0 = só hoje.
    QVector<VendaResumo> listar(int dias);
    // Itens de uma venda, para conferência.
    QVector<ItemVendido> itens(int vendaId);

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
