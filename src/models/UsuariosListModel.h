#pragma once

#include "domain/usuarios/Usuario.h"

#include <QAbstractListModel>
#include <QVector>

class UsuariosListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NomeRole,
        LoginRole,
        PerfilRole,
    };

    explicit UsuariosListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setUsuarios(const QVector<Usuario> &usuarios);

private:
    QVector<Usuario> m_usuarios;
};
