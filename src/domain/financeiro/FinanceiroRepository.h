#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct ContaPagar
{
    int id = 0;
    QString descricao;
    qint64 valor = 0;
    QString vencimento;
    QString status;
    QString fornecedorNome;
    bool vencida = false;
};

struct ContaReceber
{
    int id = 0;
    QString clienteNome;
    qint64 valor = 0;
    QString vencimento;
    QString status;
    bool vencida = false;
};

struct ResumoFinanceiro
{
    qint64 totalAPagar = 0;    // contas a pagar abertas
    qint64 totalAReceber = 0;  // contas a receber abertas
    qint64 saldoPrevisto() const { return totalAReceber - totalAPagar; }
};

// Contas a pagar/receber: listagem, baixa e despesa avulsa. "Vencida" é derivada
// (aberta e vencimento < hoje), não é um status armazenado.
class FinanceiroRepository
{
public:
    explicit FinanceiroRepository(QSqlDatabase db);

    QVector<ContaPagar> contasPagar(bool apenasAbertas);
    QVector<ContaReceber> contasReceber(bool apenasAbertas);

    bool pagar(int contaPagarId);
    bool receber(int contaReceberId);
    // Recebe um pagamento (parcial ou total) de UMA conta a receber. Se cobrir
    // o valor, marca 'paga'; senão reduz o saldo da conta. Retorna o valor
    // efetivamente abatido. NÃO abre transação própria — o chamador envolve.
    qint64 aplicarRecebimentoConta(int contaReceberId, qint64 valor);
    bool criarDespesa(const QString &descricao, qint64 valor, const QString &vencimento);

    ResumoFinanceiro resumo();

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
