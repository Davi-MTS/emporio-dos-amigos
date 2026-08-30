#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Item de estoque para listagem (uma linha por produto).
struct ItemEstoque
{
    int produtoId = 0;
    QString nome;
    QString localizacao;
    QString unidadeBase;
    qint64 quantidade = 0;   // unidade base
    int minimo = 0;          // unidade base
    qint64 custoMedio = 0;   // centavos por unidade base
    bool temFoto = false;    // evita pedir imagem de quem não tem
};

// Acesso a dados de estoque: listagem, entrada de mercadoria (com custo médio
// ponderado) e ajuste/inventário. Toda alteração de quantidade registra uma
// linha em movimentacoes_estoque (auditoria). Quantidades SEMPRE em unidade base.
class EstoqueRepository
{
public:
    explicit EstoqueRepository(QSqlDatabase db);

    QVector<ItemEstoque> listar(const QString &filtro = QString());
    ItemEstoque item(int produtoId);

    // PRECISÃO DE CUSTO: internamente o custo por unidade base é guardado em
    // MILÉSIMOS DE CENTAVO (centavos × 1000). Isso evita perder a fração de
    // centavo em unidades muito granulares (ex.: custo por ml). `ItemEstoque`,
    // as APIs em centavos e o relatório convertem nas bordas (÷1000 / ×1000).

    // Entrada de mercadoria (custo em CENTAVOS por unidade base; -1 mantém o
    // custo atual). Conveniência que converte para milésimos e delega. Em transação.
    bool registrarEntrada(int produtoId, qint64 qtdBase, qint64 custoUnitBaseCentavos,
                          int usuarioId, const QString &observacao);

    // Igual à anterior, mas com o custo já em MILÉSIMOS de centavo (-1 mantém).
    // Use quando o custo por unidade for sub-centavo (ex.: ml). Em transação.
    bool registrarEntradaMilli(int produtoId, qint64 qtdBase, qint64 custoUnitBaseMilli,
                               int usuarioId, const QString &observacao);

    // Inventário: define a quantidade contada (absoluta). Registra a diferença
    // como movimentação tipo 'inventario'. Não altera custo.
    bool registrarInventario(int produtoId, qint64 novaQtdBase,
                             const QString &motivo, int usuarioId);

    // Retirada/saída manual (perda, quebra, consumo próprio): baixa `qtdBase` do
    // estoque e registra movimentação tipo 'ajuste' (negativa). Não altera o
    // custo médio. Erro se pedir mais do que há. Em transação.
    bool registrarSaida(int produtoId, qint64 qtdBase, const QString &motivo,
                        int usuarioId);

    // Aplica uma entrada SEM abrir transação (para compor com outra operação
    // transacional, ex.: registro de compra). Custo em MILÉSIMOS de centavo
    // (-1 mantém). O chamador controla a transação.
    bool aplicarEntrada(int produtoId, qint64 qtdBase, qint64 custoUnitBaseMilli,
                        int usuarioId, const QString &origem, const QString &observacao);

    QString ultimoErro() const { return m_erro; }

private:
    bool garantirLinhaEstoque(int produtoId);

    QSqlDatabase m_db;
    QString m_erro;
};
