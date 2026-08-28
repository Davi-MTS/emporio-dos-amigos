#include "models/VendasListModel.h"

VendasListModel::VendasListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int VendasListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_vendas.size();
}

QVariant VendasListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_vendas.size())
        return {};
    const VendaResumo &v = m_vendas.at(index.row());
    switch (role) {
    case IdRole:       return v.id;
    case DataRole:     return v.data;
    case ClienteRole:  return v.clienteNome;
    case StatusRole:   return v.status;
    case FormasRole:   return v.formas;
    case TotalRole:    return v.total;
    case NumItensRole: return v.numItens;
    case MotivoRole:   return v.motivoCancelamento;
    default:           return {};
    }
}

QHash<int, QByteArray> VendasListModel::roleNames() const
{
    return {
        {IdRole, "idVenda"},
        {DataRole, "dataVenda"},
        {ClienteRole, "cliente"},
        {StatusRole, "status"},
        {FormasRole, "formas"},
        {TotalRole, "total"},
        {NumItensRole, "numItens"},
        {MotivoRole, "motivo"},
    };
}

void VendasListModel::setVendas(const QVector<VendaResumo> &vendas)
{
    beginResetModel();
    m_vendas = vendas;
    endResetModel();
}
