#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

// Resumo financeiro de uma sessão de caixa (tudo em centavos).
struct ResumoCaixa
{
    int sessaoId = 0;
    qint64 abertura = 0;
    qint64 vendasDinheiro = 0;
    qint64 vendasPix = 0;
    qint64 vendasDebito = 0;
    qint64 vendasCredito = 0;
    qint64 vendasFiado = 0;
    qint64 suprimentos = 0;
    qint64 sangrias = 0;
    qint64 recebimentos = 0; // fiado recebido em dinheiro durante o turno
    qint64 troco = 0;
    qint64 totalVendido = 0; // soma de vendas.total (o que foi realmente vendido)
    int numVendas = 0;

    // O que foi vendido no turno. Vem de `vendas.total`, NÃO da soma dos
    // pagamentos — pagamento em dinheiro pode incluir o valor entregue a mais
    // (que volta como troco) e inflaria o total.
    qint64 totalVendas() const { return totalVendido; }

    // Soma do que foi entregue/registrado por forma (dinheiro inclui o troco).
    qint64 totalRecebidoPorForma() const {
        return vendasDinheiro + vendasPix + vendasDebito + vendasCredito + vendasFiado;
    }
    // Dinheiro que deve estar na gaveta.
    qint64 dinheiroEsperado() const {
        return abertura + vendasDinheiro - troco + suprimentos - sangrias + recebimentos;
    }
};

struct ResultadoFechamento
{
    bool ok = false;
    qint64 esperado = 0;   // dinheiro esperado
    qint64 informado = 0;  // dinheiro contado
    qint64 diferenca = 0;  // informado - esperado (negativo = falta)
    QString erro;
};

// Sessões de caixa (turnos): abrir, movimentar (sangria/suprimento), resumir e
// fechar com esperado × informado × diferença. Parte crítica — sem erro silencioso.
class CaixaRepository
{
public:
    explicit CaixaRepository(QSqlDatabase db);

    int sessaoAbertaId();
    int abrirSessao(qint64 valorAbertura, int usuarioId);

    // tipo: "sangria" | "suprimento".
    bool registrarMovimento(int sessaoId, const QString &tipo, qint64 valor,
                            const QString &motivo, int usuarioId);

    // Recebimento de fiado em dinheiro (entra na gaveta). NÃO abre transação
    // própria — deve ser chamado dentro de uma transação do chamador, junto da
    // baixa da conta a receber, para ser atômico.
    bool registrarRecebimento(int sessaoId, qint64 valor, const QString &motivo,
                              int usuarioId);

    ResumoCaixa resumo(int sessaoId);

    // Fecha a sessão: grava esperado/informado/diferença e marca 'fechada'.
    ResultadoFechamento fechar(int sessaoId, qint64 dinheiroContado, int usuarioId);

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
