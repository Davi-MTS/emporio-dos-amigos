#pragma once

#include "domain/clientes/Cliente.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// CRUD de clientes + saldo de fiado (contas a receber abertas) e quitação.
class ClienteRepository
{
public:
    explicit ClienteRepository(QSqlDatabase db);

    QVector<Cliente> listar(const QString &filtro = QString());
    std::optional<Cliente> obter(int id);
    bool salvar(Cliente &cliente);
    bool inativar(int id);

    qint64 saldoDevedor(int clienteId);

    // Quita todas as contas a receber abertas do cliente. Retorna o total quitado.
    qint64 quitar(int clienteId);

    // Recebe um pagamento parcial (ou total) do cliente, abatendo as contas
    // abertas da mais antiga para a mais nova (FIFO). A última conta coberta só
    // em parte tem seu valor reduzido; as cobertas por inteiro viram 'paga'.
    // Retorna quanto foi efetivamente abatido (<= valor, limitado ao saldo).
    // NÃO abre transação própria — o chamador deve envolvê-la.
    qint64 aplicarRecebimento(int clienteId, qint64 valor);

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
