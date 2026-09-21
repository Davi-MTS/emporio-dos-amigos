#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <optional>

// Item de estoque para listagem (uma linha por produto).
struct ItemEstoque
{
    int produtoId = 0;
    QString nome;
    QString localizacao;
    QString unidadeBase;
    qint64 quantidade = 0;   // unidade base
    int minimo = 0;          // unidade base
    qint64 custoMedio = 0;   // centavos por unidade base (ARREDONDADO — só para exibir)
    // O custo de verdade, em milésimos de centavo. Qualquer conta (custo de uma
    // caixa, valor do estoque) sai daqui: em ml o custo é fração de centavo e,
    // truncado, a garrafa de 1 L de R$ 18,99 virava R$ 10,00.
    qint64 custoMedioMilli = 0;
    bool temFoto = false;    // evita pedir imagem de quem não tem

    // Preço de venda por unidade base, em MILÉSIMOS de centavo. Sai da MENOR
    // embalagem que tem preço (normalmente a unidade), levado para a unidade
    // base — a mesma referência que o aviso de custo usa, para que os dois
    // nunca discordem sobre "o preço" de um produto. 0 = produto sem preço.
    qint64 precoBaseMilli = 0;

    // Margem sobre o PREÇO DE VENDA — não é markup. (preço − custo) ÷ preço:
    // custo R$ 10,00 e venda R$ 15,00 dá 33,3% (o markup seria 50%). É o número
    // que o dono usa para saber quanto de cada real vendido sobra.
    //
    // Em DÉCIMOS de por cento (333 = 33,3%), para não espalhar double pelo
    // sistema. Devolve nada quando não há o que calcular: produto sem preço de
    // venda, ou com custo 0 — que aqui significa DESCONHECIDO (ver
    // aplicarEntrada), e mostrar 100% de margem nesse caso seria mentira.
    // Custo acima do preço devolve margem NEGATIVA de propósito: é prejuízo, e
    // é justamente o que se quer enxergar.
    std::optional<int> margemDecimos() const;
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

    // Correção de custo (aba "Custo" do Estoque): troca o custo médio do produto
    // SEM mexer na quantidade. Existe porque não havia volta para um custo
    // digitado errado na compra — nem cancelar compra existe — e o erro ficava
    // no lucro para sempre (na loja: palheiro com custo de R$ 17,50 vendido a
    // R$ 2,00, prejuízo que nunca aconteceu).
    //
    // Com `corrigirVendas`, regrava também o custo TRAVADO das vendas feitas
    // desde a última entrada do produto: é o período em que o custo errado
    // estava valendo. Antes disso o custo veio de outra entrada e não é tocado.
    // Só vendas concluídas (cancelada não entra no lucro, não há o que corrigir).
    //
    // Deixa rastro: movimentação de quantidade 0 (tipo 'ajuste', origem
    // 'ajuste_custo') com o custo antigo e o novo. O relatório de lucro só lê
    // 'saida_venda', então essa linha não mexe em conta nenhuma. Em transação.
    bool ajustarCusto(int produtoId, qint64 novoCustoMilli, bool corrigirVendas,
                      int usuarioId, const QString &motivo, int *vendasCorrigidas = nullptr);

    // Quantas vendas concluídas levaram este produto desde a última entrada, e
    // a data dessa entrada ("" se nunca houve — aí são todas). É exatamente o
    // conjunto que ajustarCusto corrige: a tela mostra o número ANTES de gravar.
    int vendasDesdeUltimaEntrada(int produtoId, QString *dataUltimaEntrada = nullptr);

private:
    // Dá custo às unidades que foram VENDIDAS SEM ESTOQUE, usando o custo da
    // mercadoria que chegou. Cobre no máximo `qtdCoberta` unidades, das vendas
    // mais antigas para as mais novas. Não abre transação.
    bool acertarCustoPendente(int produtoId, qint64 qtdCoberta, qint64 custoUnitBaseMilli);

    bool garantirLinhaEstoque(int produtoId);


    QSqlDatabase m_db;
    QString m_erro;
};
