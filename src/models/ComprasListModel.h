#pragma once

#include "domain/compras/Compra.h"

#include <QAbstractListModel>
#include <QVector>

class ComprasListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        FornecedorRole,
        DataRole,
        TotalRole,
        StatusRole,
        NumItensRole,
        NumeroNotaRole,
    };

    explicit ComprasListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCompras(const QVector<CompraResumo> &compras);

private:
    QVector<CompraResumo> m_compras;
};
