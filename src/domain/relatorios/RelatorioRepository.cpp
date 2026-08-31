#include "domain/relatorios/RelatorioRepository.h"

#include <utility>

#include <QSqlQuery>

RelatorioRepository::RelatorioRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QString RelatorioRepository::filtroPeriodo(int dias, const QString &coluna)
{
    // As datas são gravadas em hora LOCAL (migration 0009), então comparamos com
    // 'now','localtime'. Usar UTC jogava a venda da noite para o dia seguinte.
    if (dias <= 0)
        return QStringLiteral("date(%1) = date('now','localtime')").arg(coluna);
    // Janela de N dias contada a partir do início do dia (não das últimas N*24h),
    // para "7 dias" incluir o dia inteiro de 7 dias atrás.
    return QStringLiteral("%1 >= date('now','localtime','-%2 days')").arg(coluna).arg(dias - 1);
}

DashboardKpis RelatorioRepository::dashboard()
{
    DashboardKpis k;
    QSqlQuery q(m_db);

    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(total),0), COUNT(*) FROM vendas "
            "WHERE status='concluida' AND date(data)=date('now','localtime')"))
        && q.next()) {
        k.vendasHoje = q.value(0).toLongLong();
        k.numVendasHoje = q.value(1).toInt();
    }
    if (k.numVendasHoje > 0)
        k.ticketMedio = k.vendasHoje / k.numVendasHoje;

    if (q.exec(QStringLiteral(
            "SELECT COUNT(*) FROM produtos p JOIN estoque e ON e.produto_id=p.id "
            "WHERE p.ativo=1 AND p.composto=0 AND e.quantidade_atual <= p.estoque_minimo"))
        && q.next())
        k.produtosEmFalta = q.value(0).toInt();

    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(valor),0) FROM contas_receber WHERE status='aberta'"))
        && q.next())
        k.aReceber = q.value(0).toLongLong();

    return k;
}

FaturamentoResumo RelatorioRepository::faturamento(int dias)
{
    FaturamentoResumo r;
    const QString filtro = filtroPeriodo(dias, QStringLiteral("data"));

    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(total),0), COUNT(*) FROM vendas "
            "WHERE status='concluida' AND %1").arg(filtro))
        && q.next()) {
        r.total = q.value(0).toLongLong();
        r.numVendas = q.value(1).toInt();
    }
    if (r.numVendas > 0)
        r.ticket = r.total / r.numVendas;

    // Custo pelas movimentações reais de saída (registram o produto exato
    // baixado — vale para produto normal e para insumos de composto). Usa o
    // custo TRAVADO no momento da venda (m.custo_unit); para linhas antigas
    // (sem custo_unit) cai para o custo médio atual.
    const QString filtroMov = filtroPeriodo(dias, QStringLiteral("m.data"));
    // custo_unit e custo_medio_unitario estão em MILÉSIMOS de centavo; ÷1000 no
    // total traz de volta para centavos.
    // O JOIN com vendas NÃO é decorativo: sem ele, uma venda CANCELADA entrava só
    // com o custo (a receita já filtrava por 'concluida') e o lucro do dia
    // aparecia negativo — foi exatamente o que apareceu na loja como -R$ 7,50.
    // A origem da movimentação é "venda:<id>", daí o SUBSTR a partir do 7º
    // caractere.
    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(-m.quantidade * "
            "        COALESCE(m.custo_unit, e.custo_medio_unitario)),0) / 1000 "
            "FROM movimentacoes_estoque m "
            "JOIN estoque e ON e.produto_id = m.produto_id "
            "JOIN vendas v ON v.id = CAST(SUBSTR(m.origem, 7) AS INTEGER) "
            "WHERE m.tipo = 'saida_venda' AND m.origem LIKE 'venda:%' "
            "  AND v.status = 'concluida' AND %1").arg(filtroMov))
        && q.next())
        r.custo = q.value(0).toLongLong();

    r.lucro = r.total - r.custo;
    return r;
}

QVector<FormaTotal> RelatorioRepository::vendasPorForma(int dias)
{
    QVector<FormaTotal> lista;
    const QString filtro = filtroPeriodo(dias, QStringLiteral("v.data"));
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT p.forma, SUM(p.valor) FROM pagamentos p "
            "JOIN vendas v ON v.id=p.venda_id "
            "WHERE v.status='concluida' AND %1 "
            "GROUP BY p.forma ORDER BY 2 DESC").arg(filtro))) {
        while (q.next())
            lista.push_back({q.value(0).toString(), q.value(1).toLongLong()});
    }
    return lista;
}

QVector<ProdutoVendido> RelatorioRepository::maisVendidos(int dias, int limite)
{
    QVector<ProdutoVendido> lista;
    const QString filtro = filtroPeriodo(dias, QStringLiteral("v.data"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT pr.nome, SUM(vi.qtd_unidade_base) AS q FROM venda_itens vi "
        "JOIN vendas v ON v.id=vi.venda_id "
        "JOIN produtos pr ON pr.id=vi.produto_id "
        "WHERE v.status='concluida' AND %1 "
        "GROUP BY vi.produto_id ORDER BY q DESC LIMIT :lim").arg(filtro));
    q.bindValue(QStringLiteral(":lim"), limite);
    if (q.exec()) {
        while (q.next())
            lista.push_back({q.value(0).toString(), q.value(1).toLongLong()});
    }
    return lista;
}

QVector<ProdutoParado> RelatorioRepository::produtosParados(int dias)
{
    QVector<ProdutoParado> lista;
    const QString filtro = filtroPeriodo(dias, QStringLiteral("v.data"));
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT p.nome, COALESCE(e.quantidade_atual,0) FROM produtos p "
            "LEFT JOIN estoque e ON e.produto_id=p.id "
            // Composto e dose não têm estoque próprio: listá-los como "parado"
            // é ruído, porque o saldo deles é sempre zero por definição.
            "WHERE p.ativo=1 AND p.composto=0 AND COALESCE(p.dose_de_produto_id,0)=0 "
            "  AND p.id NOT IN ("
            "  SELECT DISTINCT vi.produto_id FROM venda_itens vi "
            "  JOIN vendas v ON v.id=vi.venda_id "
            "  WHERE v.status='concluida' AND %1) "
            "ORDER BY p.nome COLLATE NOCASE").arg(filtro))) {
        while (q.next())
            lista.push_back({q.value(0).toString(), q.value(1).toLongLong()});
    }
    return lista;
}
