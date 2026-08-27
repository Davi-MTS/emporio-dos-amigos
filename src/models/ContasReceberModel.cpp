#include "models/ContasReceberModel.h"

ContasReceberModel::ContasReceberModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContasReceberModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_contas.size();
}

QVariant ContasReceberModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contas.size())
        return {};
    const ContaReceber &c = m_contas.at(index.row());
    switch (role) {
    case IdRole:         return c.id;
    case ClienteRole:    return c.clienteNome;
    case ValorRole:      return c.valor;
    case VencimentoRole: return c.vencimento;
    case StatusRole:     return c.status;
    case VencidaRole:    return c.vencida;
    default:             return {};
    }
}

QHash<int, QByteArray> ContasReceberModel::roleNames() const
{
    return {
        {IdRole, "idConta"}, {ClienteRole, "cliente"}, {ValorRole, "valor"},
        {VencimentoRole, "vencimento"}, {StatusRole, "status"}, {VencidaRole, "vencida"},
    };
}

void ContasReceberModel::setContas(const QVector<ContaReceber> &contas)
{
    beginResetModel();
    m_contas = contas;
    endResetModel();
}
