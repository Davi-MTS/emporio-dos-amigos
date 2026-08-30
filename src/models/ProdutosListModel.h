#pragma once

#include "domain/produtos/Produto.h"

#include <QAbstractListModel>
#include <QVector>

// Model de lista de produtos para o QML (ListView/TableView).
// Apenas adapta dados já carregados pelo ProdutoRepository — não acessa o banco.
class ProdutosListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NomeRole,
        CategoriaRole,
        EstoqueRole,
        EstoqueMinimoRole,
        PrecoRole,
        UnidadeBaseRole,
        StatusRole,        // "ok" | "baixo" | "zerado"
        CompostoRole,
        TemFotoRole,
        DoseOrigemRole,
    };

    explicit ProdutosListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setProdutos(const QVector<Produto> &produtos);

    // id do produto na linha (ou 0). Útil para abrir a edição a partir do QML.
    Q_INVOKABLE int idNaLinha(int row) const;

private:
    static QString statusDe(const Produto &p);
    QVector<Produto> m_produtos;
};
