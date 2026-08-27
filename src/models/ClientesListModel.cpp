#include "models/ClientesListModel.h"

ClientesListModel::ClientesListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ClientesListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_clientes.size();
}

QVariant ClientesListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_clientes.size())
        return {};
    const Cliente &c = m_clientes.at(index.row());
    switch (role) {
    case IdRole:       return c.id;
    case NomeRole:     return c.nome;
    case TelefoneRole: return c.telefone;
    case CpfRole:      return c.cpf;
    case LimiteRole:   return c.limiteFiado;
    case SaldoRole:    return c.saldoDevedor;
    default:           return {};
    }
}

QHash<int, QByteArray> ClientesListModel::roleNames() const
{
    return {
        {IdRole, "idCliente"},
        {NomeRole, "nome"},
        {TelefoneRole, "telefone"},
        {CpfRole, "cpf"},
        {LimiteRole, "limite"},
        {SaldoRole, "saldo"},
    };
}

void ClientesListModel::setClientes(const QVector<Cliente> &clientes)
{
    beginResetModel();
    m_clientes = clientes;
    endResetModel();
}
