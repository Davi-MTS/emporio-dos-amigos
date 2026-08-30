#include "models/EstoqueListModel.h"

EstoqueListModel::EstoqueListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EstoqueListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_itens.size();
}

QVariant EstoqueListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_itens.size())
        return {};

    const ItemEstoque &it = m_itens.at(index.row());
    switch (role) {
    case ProdutoIdRole:   return it.produtoId;
    case NomeRole:        return it.nome;
    case LocalizacaoRole: return it.localizacao;
    case QuantidadeRole:  return it.quantidade;
    case MinimoRole:      return it.minimo;
    case CustoMedioRole:  return it.custoMedio;
    case UnidadeBaseRole: return it.unidadeBase;
    case StatusRole:      return statusDe(it);
    case TemFotoRole:     return it.temFoto;
    default:              return {};
    }
}

QHash<int, QByteArray> EstoqueListModel::roleNames() const
{
    return {
        {ProdutoIdRole, "idProduto"},
        {TemFotoRole, "temFoto"},
        {NomeRole, "nome"},
        {LocalizacaoRole, "localizacao"},
        {QuantidadeRole, "quantidade"},
        {MinimoRole, "minimo"},
        {CustoMedioRole, "custoMedio"},
        {UnidadeBaseRole, "unidadeBase"},
        {StatusRole, "status"},
    };
}

void EstoqueListModel::setItens(const QVector<ItemEstoque> &itens)
{
    beginResetModel();
    m_itens = itens;
    endResetModel();
}

QString EstoqueListModel::statusDe(const ItemEstoque &it)
{
    if (it.quantidade <= 0)
        return QStringLiteral("zerado");
    if (it.quantidade <= it.minimo)
        return QStringLiteral("baixo");
    return QStringLiteral("ok");
}
