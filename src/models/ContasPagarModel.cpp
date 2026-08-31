#include "models/ContasPagarModel.h"

ContasPagarModel::ContasPagarModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContasPagarModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_contas.size();
}

QVariant ContasPagarModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contas.size())
        return {};
    const ContaPagar &c = m_contas.at(index.row());
    switch (role) {
    case IdRole:          return c.id;
    case DescricaoRole:   return c.descricao;
    case ValorRole:       return c.valor;
    case VencimentoRole:  return c.vencimento;
    case StatusRole:      return c.status;
    case FornecedorRole:  return c.fornecedorNome;
    case VencidaRole:     return c.vencida;
    case PagoEmRole:      return c.pagoEm;
    case FormaPagamentoRole: return c.formaPagamento;
    case AvulsaRole:      return c.avulsa;
    default:              return {};
    }
}

QHash<int, QByteArray> ContasPagarModel::roleNames() const
{
    return {
        {IdRole, "idConta"}, {DescricaoRole, "descricao"}, {ValorRole, "valor"},
        {VencimentoRole, "vencimento"}, {StatusRole, "status"},
        {FornecedorRole, "fornecedor"}, {VencidaRole, "vencida"},
        {PagoEmRole, "pagoEm"}, {FormaPagamentoRole, "formaPagamento"},
        {AvulsaRole, "avulsa"},
    };
}

void ContasPagarModel::setContas(const QVector<ContaPagar> &contas)
{
    beginResetModel();
    m_contas = contas;
    endResetModel();
}
