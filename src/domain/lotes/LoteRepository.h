#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Um lote é uma remessa que entrou com uma validade própria. Doce, salgadinho e
// gelo vencem; e duas cargas do MESMO produto costumam ter validades
// diferentes, então uma data por produto não resolve — perderia a anterior.
struct Lote
{
    int id = 0;
    int produtoId = 0;
    QString produtoNome;
    QString unidadeBase;
    QString codigo;          // código do lote na embalagem (opcional)
    QString validade;        // ISO date
    qint64 quantidade = 0;   // unidade base, o que ainda resta deste lote
    int diasParaVencer = 0;  // negativo = já venceu
};

// Contagem rápida para a tela e para os alertas.
struct ResumoVencimento
{
    int vencidos = 0;
    int venceEm7 = 0;
    int venceEm30 = 0;
    qint64 quantidadeVencida = 0;
};

// Controle de validade por remessa. Saídas consomem sempre o lote que vence
// PRIMEIRO (FEFO — first expired, first out): é o que o comerciante faz na
// prateleira, e o contrário deixaria mercadoria velha encalhada.
//
// Nenhum método abre transação: quem chama (venda, compra) já está dentro de uma.
class LoteRepository
{
public:
    explicit LoteRepository(QSqlDatabase db);

    // Registra a entrada de uma remessa com validade. Quantidade em unidade base.
    bool registrar(int produtoId, qint64 quantidadeBase, const QString &validade,
                   const QString &codigo = QString());

    // Baixa `quantidadeBase` do produto, dos lotes que vencem primeiro.
    // Devolve quanto CONSEGUIU baixar: pode ser menos que o pedido quando parte
    // do estoque entrou sem validade informada (aí não há lote para baixar).
    // Isso não é erro — o estoque continua sendo a fonte de verdade.
    qint64 consumirFefo(int produtoId, qint64 quantidadeBase);

    // Lotes com saldo, do que vence primeiro para o último.
    // diasLimite < 0 traz todos; 0 traz só os já vencidos.
    QVector<Lote> listar(int diasLimite = -1);

    ResumoVencimento resumo();

    // Soma dos lotes de um produto (para avisar quando divergir do estoque).
    qint64 totalEmLotes(int produtoId);

    // Produtos cuja soma de lotes não bate com o estoque. Acontece quando parte
    // entrou sem validade, ou depois de um ajuste de inventário — que mexe no
    // saldo mas não sabe de qual remessa tirar.
    QVector<QPair<QString, qint64>> divergencias();

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
