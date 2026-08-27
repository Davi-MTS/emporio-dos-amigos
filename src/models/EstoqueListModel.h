#pragma once

#include "domain/estoque/EstoqueRepository.h"

#include <QAbstractListModel>
#include <QVector>

// Model de lista de estoque para o QML.
class EstoqueListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ProdutoIdRole = Qt::UserRole + 1,
        NomeRole,
        LocalizacaoRole,
        QuantidadeRole,
        MinimoRole,
        CustoMedioRole,
        UnidadeBaseRole,
        StatusRole,   // "ok" | "baixo" | "zerado"
    };

    explicit EstoqueListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItens(const QVector<ItemEstoque> &itens);

private:
    static QString statusDe(const ItemEstoque &it);
    QVector<ItemEstoque> m_itens;
};
