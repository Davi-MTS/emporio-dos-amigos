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
    // Filtro por faixa de margem: "" (todas) | "baixa" | "boa" | "muitoboa".
    // As faixas são decisão do dono: abaixo de 35% é baixa, de 35% a 45% é boa,
    // acima de 45% é muito boa. Mora aqui pelo mesmo motivo do filtro de
    // situação — a faixa que pinta o número na tela e a que filtra a lista têm
    // de ser a MESMA regra, senão a pessoa filtra "Baixa" e vê um verde.
    Q_PROPERTY(QString filtroMargem READ filtroMargem WRITE setFiltroMargem NOTIFY filtroMargemChanged)
    // Quantos há em cada situação, sobre a lista inteira (respeitando a busca
    // por nome, não o filtro): { todos, zerado, baixo, ok }.
    Q_PROPERTY(QVariantMap contagem READ contagem NOTIFY contagemChanged)
    // { todos, baixa, boa, muitoboa, sem } — "sem" é quem não tem margem para
    // calcular, que não entra em faixa nenhuma e só aparece em "Todas".
    Q_PROPERTY(QVariantMap contagemMargem READ contagemMargem NOTIFY contagemChanged)
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
        // Margem sobre o preço de venda, em décimos de por cento (333 = 33,3%).
        // Inválido — `undefined` no QML — quando não há como calcular (produto
        // sem preço ou com custo desconhecido); a tela mostra "—" nesse caso.
        //
        // ROLE NOVO ENTRA NO FIM desta lista. O QML enxerga os roles pelo nome,
        // mas os testes chegam neles pelo NÚMERO (Qt.UserRole + posição), então
        // inserir no meio renumera tudo o que vem depois e quebra testes que
        // não têm nada a ver com a mudança.
        MargemRole,
    };

    explicit EstoqueListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItens(const QVector<ItemEstoque> &itens);

    // Margem é informação de LUCRO: quem não vê o financeiro não a recebe.
    // Desligada, o role sai inválido — quem chegar ao model por fora da tela
    // também não acha número nenhum. O AppBackend liga/desliga isto a cada
    // troca de usuário, e não só na recarga: a lista já carregada precisa
    // mudar junto quando o funcionário entra no lugar do admin.
    void setMostrarMargem(bool mostrar);

    QString filtroStatus() const { return m_filtroStatus; }
    void setFiltroStatus(const QString &status);
    QVariantMap contagem() const;

    QString filtroMargem() const { return m_filtroMargem; }
    void setFiltroMargem(const QString &faixa);
    QVariantMap contagemMargem() const;

    // Faixa de margem de um item: "baixa" (< 35%), "boa" (35% a 45%),
    // "muitoboa" (> 45%) ou "" para quem não tem margem calculável.
    static QString faixaMargemDe(const ItemEstoque &it);

signals:
    void filtroStatusChanged();
    void filtroMargemChanged();
    void contagemChanged();

private:
    static QString statusDe(const ItemEstoque &it);
    void aplicarFiltro();

    QVector<ItemEstoque> m_todos;   // tudo o que veio do banco (após a busca)
    QVector<ItemEstoque> m_itens;   // o que aparece (após o filtro de status)
    QString m_filtroStatus;
    QString m_filtroMargem;
    // Começa DESLIGADA: no boot ainda não há ninguém logado, e o padrão seguro
    // é não mostrar lucro para quem ainda não se identificou.
    bool m_mostrarMargem = false;
};
