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
    m_todos = itens;
    aplicarFiltro();
    emit contagemChanged();
}

void EstoqueListModel::setFiltroStatus(const QString &status)
{
    // Qualquer valor fora dos três vira "todos": um filtro desconhecido não pode
    // esconder a lista inteira sem explicação.
    const QString valido = (status == QLatin1String("zerado") || status == QLatin1String("baixo")
                            || status == QLatin1String("ok"))
                               ? status : QString();
    if (valido == m_filtroStatus)
        return;
    m_filtroStatus = valido;
    aplicarFiltro();
    emit filtroStatusChanged();
}

void EstoqueListModel::aplicarFiltro()
{
    beginResetModel();
    if (m_filtroStatus.isEmpty()) {
        m_itens = m_todos;
    } else {
        m_itens.clear();
        for (const ItemEstoque &it : std::as_const(m_todos)) {
            if (statusDe(it) == m_filtroStatus)
                m_itens.push_back(it);
        }
    }
    endResetModel();
}

QVariantMap EstoqueListModel::contagem() const
{
    int zerado = 0, baixo = 0, ok = 0;
    for (const ItemEstoque &it : m_todos) {
        const QString s = statusDe(it);
        if (s == QLatin1String("zerado"))
            ++zerado;
        else if (s == QLatin1String("baixo"))
            ++baixo;
        else
            ++ok;
    }
    return {
        {QStringLiteral("todos"), static_cast<int>(m_todos.size())},
        {QStringLiteral("zerado"), zerado},
        {QStringLiteral("baixo"), baixo},
        {QStringLiteral("ok"), ok},
    };
}

QString EstoqueListModel::statusDe(const ItemEstoque &it)
{
    if (it.quantidade <= 0)
        return QStringLiteral("zerado");
    if (it.quantidade <= it.minimo)
        return QStringLiteral("baixo");
    return QStringLiteral("ok");
}
