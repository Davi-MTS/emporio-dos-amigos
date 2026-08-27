#include "models/FornecedoresListModel.h"

FornecedoresListModel::FornecedoresListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FornecedoresListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_fornecedores.size();
}

QVariant FornecedoresListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_fornecedores.size())
        return {};
    const Fornecedor &f = m_fornecedores.at(index.row());
    switch (role) {
    case IdRole:       return f.id;
    case NomeRole:     return f.nome;
    case CnpjRole:     return f.cnpj;
    case TelefoneRole: return f.telefone;
    default:           return {};
    }
}

QHash<int, QByteArray> FornecedoresListModel::roleNames() const
{
    return {
        {IdRole, "idFornecedor"},
        {NomeRole, "nome"},
        {CnpjRole, "cnpj"},
        {TelefoneRole, "telefone"},
    };
}

void FornecedoresListModel::setFornecedores(const QVector<Fornecedor> &fornecedores)
{
    beginResetModel();
    m_fornecedores = fornecedores;
    endResetModel();
}
