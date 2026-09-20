#pragma once

#include <QDate>
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

// Período de um relatório: os últimos `dias` dias contando hoje (dias <= 0 =>
// só hoje) OU um dia específico, quando `dia` é válido. O dia específico existe
// para responder "quanto vendi no sábado?" sem precisar abrir 7 dias e fazer
// conta de cabeça.
struct Periodo
{
    int dias = 0;
    QDate dia;

    static Periodo ultimosDias(int d) { Periodo p; p.dias = d; return p; }
    static Periodo doDia(const QDate &d) { Periodo p; p.dia = d; return p; }
};

// Consultas agregadas para o dashboard e relatórios. Só considera vendas
// concluídas. Custo/lucro são estimados pelo custo médio ATUAL (o custo no
// momento da venda não é armazenado).
class RelatorioRepository
{
public:
    explicit RelatorioRepository(QSqlDatabase db);

    DashboardKpis dashboard();

    FaturamentoResumo faturamento(const Periodo &periodo);
    QVector<FormaTotal> vendasPorForma(const Periodo &periodo);
    QVector<ProdutoVendido> maisVendidos(const Periodo &periodo, int limite);
    QVector<ProdutoParado> produtosParados(const Periodo &periodo);

    // Atalhos por número de dias (dias <= 0 => apenas hoje) — o que o Dashboard
    // e o relatório do celular sempre usaram.
    FaturamentoResumo faturamento(int dias) { return faturamento(Periodo::ultimosDias(dias)); }
    QVector<FormaTotal> vendasPorForma(int dias) { return vendasPorForma(Periodo::ultimosDias(dias)); }
    QVector<ProdutoVendido> maisVendidos(int dias, int limite)
    { return maisVendidos(Periodo::ultimosDias(dias), limite); }
    QVector<ProdutoParado> produtosParados(int dias) { return produtosParados(Periodo::ultimosDias(dias)); }

private:
    static QString filtroPeriodo(const Periodo &periodo, const QString &coluna);

    QSqlDatabase m_db;
};
