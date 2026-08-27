#include "models/ComprasListModel.h"

ComprasListModel::ComprasListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ComprasListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_compras.size();
}

QVariant ComprasListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_compras.size())
        return {};
    const CompraResumo &c = m_compras.at(index.row());
    switch (role) {
    case IdRole:         return c.id;
    case FornecedorRole: return c.fornecedorNome;
    case DataRole:       return c.data;
    case TotalRole:      return c.total;
    case StatusRole:     return c.status;
    case NumItensRole:   return c.numItens;
    case NumeroNotaRole: return c.numeroNota;
    default:             return {};
    }
}

QHash<int, QByteArray> ComprasListModel::roleNames() const
{
    return {
        {IdRole, "idCompra"},
        {FornecedorRole, "fornecedor"},
        {DataRole, "dataCompra"},
        {TotalRole, "total"},
        {StatusRole, "status"},
        {NumItensRole, "numItens"},
        {NumeroNotaRole, "numeroNota"},
    };
}

void ComprasListModel::setCompras(const QVector<CompraResumo> &compras)
{
    beginResetModel();
    m_compras = compras;
    endResetModel();
}
