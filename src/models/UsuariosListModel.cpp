#include "models/UsuariosListModel.h"

UsuariosListModel::UsuariosListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int UsuariosListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_usuarios.size();
}

QVariant UsuariosListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_usuarios.size())
        return {};
    const Usuario &u = m_usuarios.at(index.row());
    switch (role) {
    case IdRole:     return u.id;
    case NomeRole:   return u.nome;
    case LoginRole:  return u.login;
    case PerfilRole: return u.perfilNome;
    default:         return {};
    }
}

QHash<int, QByteArray> UsuariosListModel::roleNames() const
{
    return {
        {IdRole, "idUsuario"},
        {NomeRole, "nome"},
        {LoginRole, "login"},
        {PerfilRole, "perfil"},
    };
}

void UsuariosListModel::setUsuarios(const QVector<Usuario> &usuarios)
{
    beginResetModel();
    m_usuarios = usuarios;
    endResetModel();
}
