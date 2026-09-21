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
    case MargemRole: {
        if (!m_mostrarMargem)
            return {};
        const std::optional<int> m = it.margemDecimos();
        return m ? QVariant(*m) : QVariant();   // sem margem => undefined no QML
    }
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
        {MargemRole, "margem"},
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

void EstoqueListModel::setMostrarMargem(bool mostrar)
{
    if (mostrar == m_mostrarMargem)
        return;
    m_mostrarMargem = mostrar;
    // Quem não vê margem também não pode ficar com um filtro de margem ligado:
    // a lista apareceria encurtada sem nada na tela explicando por quê.
    if (!mostrar && !m_filtroMargem.isEmpty())
        setFiltroMargem(QString());
    if (!m_itens.isEmpty())
        emit dataChanged(index(0, 0), index(m_itens.size() - 1, 0), {MargemRole});
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
    emit contagemChanged();   // a contagem por faixa de margem é sobre esta situação
}

void EstoqueListModel::setFiltroMargem(const QString &faixa)
{
    // Igual ao filtro de situação: valor fora das três faixas vira "todas".
    // Um filtro desconhecido não pode esvaziar a lista sem explicação.
    const QString valido = (faixa == QLatin1String("baixa") || faixa == QLatin1String("boa")
                            || faixa == QLatin1String("muitoboa"))
                               ? faixa : QString();
    if (valido == m_filtroMargem)
        return;
    m_filtroMargem = valido;
    aplicarFiltro();
    emit filtroMargemChanged();
    emit contagemChanged();   // a contagem por situação passa a ser sobre esta faixa
}

// Os dois filtros valem JUNTOS: "o que está acabando e ainda por cima vende com
// margem baixa" é a pergunta que resolve a compra da semana.
void EstoqueListModel::aplicarFiltro()
{
    beginResetModel();
    m_itens.clear();
    for (const ItemEstoque &it : std::as_const(m_todos)) {
        if (!m_filtroStatus.isEmpty() && statusDe(it) != m_filtroStatus)
            continue;
        if (!m_filtroMargem.isEmpty() && faixaMargemDe(it) != m_filtroMargem)
            continue;
        m_itens.push_back(it);
    }
    endResetModel();
}

// Cada contagem é feita sobre a lista JÁ filtrada pelo outro filtro — é o
// número que se vai encontrar ao clicar no botão. Contar sobre a lista inteira
// mostraria "Baixa 4" e entregaria uma lista vazia quando o outro filtro
// estivesse ligado.
QVariantMap EstoqueListModel::contagem() const
{
    int zerado = 0, baixo = 0, ok = 0, todos = 0;
    for (const ItemEstoque &it : m_todos) {
        if (!m_filtroMargem.isEmpty() && faixaMargemDe(it) != m_filtroMargem)
            continue;
        ++todos;
        const QString s = statusDe(it);
        if (s == QLatin1String("zerado"))
            ++zerado;
        else if (s == QLatin1String("baixo"))
            ++baixo;
        else
            ++ok;
    }
    return {
        {QStringLiteral("todos"), todos},
        {QStringLiteral("zerado"), zerado},
        {QStringLiteral("baixo"), baixo},
        {QStringLiteral("ok"), ok},
    };
}

QVariantMap EstoqueListModel::contagemMargem() const
{
    int baixa = 0, boa = 0, muitoboa = 0, sem = 0, todos = 0;
    for (const ItemEstoque &it : m_todos) {
        if (!m_filtroStatus.isEmpty() && statusDe(it) != m_filtroStatus)
            continue;
        ++todos;
        const QString f = faixaMargemDe(it);
        if (f == QLatin1String("baixa"))
            ++baixa;
        else if (f == QLatin1String("boa"))
            ++boa;
        else if (f == QLatin1String("muitoboa"))
            ++muitoboa;
        else
            ++sem;
    }
    return {
        {QStringLiteral("todos"), todos},
        {QStringLiteral("baixa"), baixa},
        {QStringLiteral("boa"), boa},
        {QStringLiteral("muitoboa"), muitoboa},
        {QStringLiteral("sem"), sem},
    };
}

QString EstoqueListModel::faixaMargemDe(const ItemEstoque &it)
{
    const std::optional<int> m = it.margemDecimos();   // décimos de por cento
    if (!m)
        return {};        // sem preço ou sem custo: não entra em faixa nenhuma
    if (*m < 350)
        return QStringLiteral("baixa");       // abaixo de 35% (inclui prejuízo)
    if (*m <= 450)
        return QStringLiteral("boa");         // de 35% a 45%
    return QStringLiteral("muitoboa");        // acima de 45%
}

QString EstoqueListModel::statusDe(const ItemEstoque &it)
{
    if (it.quantidade <= 0)
        return QStringLiteral("zerado");
    if (it.quantidade <= it.minimo)
        return QStringLiteral("baixo");
    return QStringLiteral("ok");
}
