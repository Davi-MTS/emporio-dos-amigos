#pragma once

#include "domain/compras/Fornecedor.h"

#include <QAbstractListModel>
#include <QVector>

class FornecedoresListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NomeRole,
        CnpjRole,
        TelefoneRole,
    };

    explicit FornecedoresListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFornecedores(const QVector<Fornecedor> &fornecedores);

private:
    QVector<Fornecedor> m_fornecedores;
};
