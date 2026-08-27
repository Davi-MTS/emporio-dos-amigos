#pragma once

#include "domain/financeiro/FinanceiroRepository.h"

#include <QAbstractListModel>
#include <QVector>

class ContasReceberModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        ClienteRole, ValorRole, VencimentoRole, StatusRole, VencidaRole,
    };
    explicit ContasReceberModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setContas(const QVector<ContaReceber> &contas);

private:
    QVector<ContaReceber> m_contas;
};
