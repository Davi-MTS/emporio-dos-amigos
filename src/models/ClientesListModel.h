#pragma once

#include "domain/clientes/Cliente.h"

#include <QAbstractListModel>
#include <QVector>

class ClientesListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NomeRole,
        TelefoneRole,
        CpfRole,
        LimiteRole,
        SaldoRole,
    };

    explicit ClientesListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setClientes(const QVector<Cliente> &clientes);

private:
    QVector<Cliente> m_clientes;
};
