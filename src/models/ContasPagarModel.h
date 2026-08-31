#pragma once

#include "domain/financeiro/FinanceiroRepository.h"

#include <QAbstractListModel>
#include <QVector>

class ContasPagarModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        DescricaoRole, ValorRole, VencimentoRole, StatusRole, FornecedorRole, VencidaRole,
        PagoEmRole, FormaPagamentoRole, AvulsaRole,
    };
    explicit ContasPagarModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setContas(const QVector<ContaPagar> &contas);

private:
    QVector<ContaPagar> m_contas;
};
