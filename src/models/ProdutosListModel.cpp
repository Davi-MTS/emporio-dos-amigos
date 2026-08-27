#include "models/ProdutosListModel.h"

ProdutosListModel::ProdutosListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ProdutosListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_produtos.size();
}

QVariant ProdutosListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_produtos.size())
        return {};

    const Produto &p = m_produtos.at(index.row());
    switch (role) {
    case IdRole:            return p.id;
    case NomeRole:          return p.nome;
    case CategoriaRole:     return p.categoriaNome;
    case EstoqueRole:       return p.quantidadeEstoque;
    case EstoqueMinimoRole: return p.estoqueMinimo;
    case PrecoRole:         return p.precoPrincipal;
    case UnidadeBaseRole:   return p.unidadeBase;
    case StatusRole:        return statusDe(p);
    case CompostoRole:      return p.composto;
    default:                return {};
    }
}

QHash<int, QByteArray> ProdutosListModel::roleNames() const
{
    return {
        {IdRole, "idProduto"},
        {NomeRole, "nome"},
        {CategoriaRole, "categoria"},
        {EstoqueRole, "estoque"},
        {EstoqueMinimoRole, "estoqueMinimo"},
        {PrecoRole, "preco"},
        {UnidadeBaseRole, "unidadeBase"},
        {StatusRole, "status"},
        {CompostoRole, "composto"},
    };
}

void ProdutosListModel::setProdutos(const QVector<Produto> &produtos)
{
    beginResetModel();
    m_produtos = produtos;
    endResetModel();
}

int ProdutosListModel::idNaLinha(int row) const
{
    if (row < 0 || row >= m_produtos.size())
        return 0;
    return m_produtos.at(row).id;
}

QString ProdutosListModel::statusDe(const Produto &p)
{
    if (p.quantidadeEstoque <= 0)
        return QStringLiteral("zerado");
    if (p.quantidadeEstoque <= p.estoqueMinimo)
        return QStringLiteral("baixo");
    return QStringLiteral("ok");
}
