#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct DashboardKpis
{
    qint64 vendasHoje = 0;
    int numVendasHoje = 0;
    qint64 ticketMedio = 0;
    int produtosEmFalta = 0;
    qint64 aReceber = 0;
};

struct FaturamentoResumo
{
    qint64 total = 0;
    int numVendas = 0;
    qint64 ticket = 0;
    qint64 custo = 0;
    qint64 lucro = 0;
};

struct FormaTotal { QString forma; qint64 total = 0; };
struct ProdutoVendido { QString nome; qint64 qtd = 0; };
struct ProdutoParado { QString nome; qint64 estoque = 0; };

// Consultas agregadas para o dashboard e relatórios. Só considera vendas
// concluídas. Custo/lucro são estimados pelo custo médio ATUAL (o custo no
// momento da venda não é armazenado).
class RelatorioRepository
{
public:
    explicit RelatorioRepository(QSqlDatabase db);

    DashboardKpis dashboard();

    // dias <= 0 => apenas hoje; senão, últimos `dias` dias.
    FaturamentoResumo faturamento(int dias);
    QVector<FormaTotal> vendasPorForma(int dias);
    QVector<ProdutoVendido> maisVendidos(int dias, int limite);
    QVector<ProdutoParado> produtosParados(int dias);

private:
    static QString filtroPeriodo(int dias, const QString &coluna);

    QSqlDatabase m_db;
};
