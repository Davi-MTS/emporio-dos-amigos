#pragma once

#include "domain/vendas/VendaRepository.h"

#include <QAbstractListModel>
#include <QVector>

// Histórico de vendas para o QML.
class VendasListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        DataRole,
        ClienteRole,
        StatusRole,
        FormasRole,
        TotalRole,
        NumItensRole,
        MotivoRole,
    };

    explicit VendasListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setVendas(const QVector<VendaResumo> &vendas);

private:
    QVector<VendaResumo> m_vendas;
};
