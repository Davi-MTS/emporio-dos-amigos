#pragma once

#include "domain/estoque/EstoqueRepository.h"

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

// Model de lista de estoque para o QML.
class EstoqueListModel : public QAbstractListModel
{
    Q_OBJECT
    // Filtro por situação do estoque: "" (todos) | "zerado" | "baixo" | "ok".
    // Fica no model, e não na tela, por dois motivos: usa a MESMA regra do selo
    // da coluna Status (statusDe), então filtro e selo nunca discordam; e
    // sobrevive às recargas — dar entrada num produto "Zerado" recarrega a lista
    // e ele sai do filtro sozinho, que é o retorno que se quer ver.
    Q_PROPERTY(QString filtroStatus READ filtroStatus WRITE setFiltroStatus NOTIFY filtroStatusChanged)
    // Quantos há em cada situação, sobre a lista inteira (respeitando a busca
    // por nome, não o filtro): { todos, zerado, baixo, ok }.
    Q_PROPERTY(QVariantMap contagem READ contagem NOTIFY contagemChanged)
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
        TemFotoRole,
    };

    explicit EstoqueListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItens(const QVector<ItemEstoque> &itens);

    QString filtroStatus() const { return m_filtroStatus; }
    void setFiltroStatus(const QString &status);
    QVariantMap contagem() const;

signals:
    void filtroStatusChanged();
    void contagemChanged();

private:
    static QString statusDe(const ItemEstoque &it);
    void aplicarFiltro();

    QVector<ItemEstoque> m_todos;   // tudo o que veio do banco (após a busca)
    QVector<ItemEstoque> m_itens;   // o que aparece (após o filtro de status)
    QString m_filtroStatus;
};
