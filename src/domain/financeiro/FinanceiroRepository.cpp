#include "domain/financeiro/FinanceiroRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

FinanceiroRepository::FinanceiroRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<ContaPagar> FinanceiroRepository::contasPagar(bool apenasAbertas)
{
    QVector<ContaPagar> lista;
    QString sql = QStringLiteral(
        "SELECT cp.id, "
        "  COALESCE(cp.descricao, 'Compra #' || cp.compra_id), "
        "  cp.valor, cp.vencimento, cp.status, COALESCE(f.nome, '—'), "
        "  CASE WHEN cp.status='aberta' AND cp.vencimento IS NOT NULL "
        "       AND cp.vencimento < date('now','localtime') THEN 1 ELSE 0 END, "
        "  COALESCE(cp.pago_em, ''), COALESCE(cp.forma_pagamento, '') "
        "FROM contas_pagar cp "
        "LEFT JOIN compras c ON c.id = cp.compra_id "
        "LEFT JOIN fornecedores f ON f.id = c.fornecedor_id ");
    if (apenasAbertas)
        sql += QStringLiteral("WHERE cp.status = 'aberta' ");
    sql += QStringLiteral("ORDER BY cp.vencimento IS NULL, cp.vencimento ASC, cp.id DESC");

    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        ContaPagar c;
        c.id = q.value(0).toInt();
        c.descricao = q.value(1).toString();
        c.valor = q.value(2).toLongLong();
        c.vencimento = q.value(3).toString();
        c.status = q.value(4).toString();
        c.fornecedorNome = q.value(5).toString();
        c.vencida = q.value(6).toInt() != 0;
        c.pagoEm = q.value(7).toString();
        c.formaPagamento = q.value(8).toString();
        lista.push_back(c);
    }
    return lista;
}

QVector<ContaReceber> FinanceiroRepository::contasReceber(bool apenasAbertas)
{
    QVector<ContaReceber> lista;
    QString sql = QStringLiteral(
        "SELECT cr.id, COALESCE(cl.nome, '—'), cr.valor, cr.vencimento, cr.status, "
        "  CASE WHEN cr.status='aberta' AND cr.vencimento IS NOT NULL "
        "       AND cr.vencimento < date('now','localtime') THEN 1 ELSE 0 END "
        "FROM contas_receber cr "
        "LEFT JOIN clientes cl ON cl.id = cr.cliente_id ");
    if (apenasAbertas)
        sql += QStringLiteral("WHERE cr.status = 'aberta' ");
    sql += QStringLiteral("ORDER BY cr.vencimento IS NULL, cr.vencimento ASC, cr.id DESC");

    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        ContaReceber c;
        c.id = q.value(0).toInt();
        c.clienteNome = q.value(1).toString();
        c.valor = q.value(2).toLongLong();
        c.vencimento = q.value(3).toString();
        c.status = q.value(4).toString();
        c.vencida = q.value(5).toInt() != 0;
        lista.push_back(c);
    }
    return lista;
}

bool FinanceiroRepository::pagar(int contaPagarId, const QString &forma)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE contas_pagar SET status='paga', pago_em=date('now','localtime'), "
        "forma_pagamento=:forma WHERE id=:id AND status='aberta'"));
    q.bindValue(QStringLiteral(":forma"), forma.isEmpty() ? QVariant() : QVariant(forma));
    q.bindValue(QStringLiteral(":id"), contaPagarId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() <= 0) {
        m_erro = QStringLiteral("Conta não encontrada ou já paga.");
        return false;
    }
    return true;
}

bool FinanceiroRepository::estornarPagamento(int contaPagarId, QString *forma,
                                             qint64 *valor, QString *descricao)
{
    QSqlQuery leitura(m_db);
    leitura.prepare(QStringLiteral(
        "SELECT valor, COALESCE(forma_pagamento, ''), "
        "       COALESCE(descricao, 'Compra #' || compra_id) "
        "FROM contas_pagar WHERE id = :id AND status = 'paga'"));
    leitura.bindValue(QStringLiteral(":id"), contaPagarId);
    if (!leitura.exec() || !leitura.next()) {
        m_erro = QStringLiteral("Esta conta não está paga.");
        return false;
    }
    if (valor)     *valor = leitura.value(0).toLongLong();
    if (forma)     *forma = leitura.value(1).toString();
    if (descricao) *descricao = leitura.value(2).toString();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE contas_pagar SET status='aberta', pago_em=NULL, forma_pagamento=NULL "
        "WHERE id=:id AND status='paga'"));
    q.bindValue(QStringLiteral(":id"), contaPagarId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() <= 0) {
        m_erro = QStringLiteral("Não foi possível estornar esta conta.");
        return false;
    }
    return true;
}

bool FinanceiroRepository::receber(int contaReceberId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE contas_receber SET status='paga', pago_em=date('now','localtime') "
        "WHERE id=:id AND status='aberta'"));
    q.bindValue(QStringLiteral(":id"), contaReceberId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

qint64 FinanceiroRepository::aplicarRecebimentoConta(int contaReceberId, qint64 valor)
{
    if (valor <= 0)
        return 0;

    qint64 atual = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT valor FROM contas_receber WHERE id = :id AND status = 'aberta'"));
        q.bindValue(QStringLiteral(":id"), contaReceberId);
        if (!q.exec() || !q.next())
            return 0; // conta inexistente ou já paga
        atual = q.value(0).toLongLong();
    }

    const qint64 aplicado = qMin(valor, atual);
    QSqlQuery u(m_db);
    if (aplicado >= atual) {
        u.prepare(QStringLiteral(
            "UPDATE contas_receber SET status = 'paga', pago_em = date('now','localtime') WHERE id = :id"));
        u.bindValue(QStringLiteral(":id"), contaReceberId);
    } else {
        u.prepare(QStringLiteral(
            "UPDATE contas_receber SET valor = valor - :p WHERE id = :id"));
        u.bindValue(QStringLiteral(":p"), aplicado);
        u.bindValue(QStringLiteral(":id"), contaReceberId);
    }
    if (!u.exec()) {
        m_erro = u.lastError().text();
        return 0;
    }
    return aplicado;
}

bool FinanceiroRepository::criarDespesa(const QString &descricao, qint64 valor,
                                        const QString &vencimento)
{
    if (descricao.trimmed().isEmpty()) {
        m_erro = QStringLiteral("Informe a descrição da despesa.");
        return false;
    }
    if (valor <= 0) {
        m_erro = QStringLiteral("O valor deve ser maior que zero.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO contas_pagar (descricao, valor, vencimento, status) "
        "VALUES (:desc, :valor, :venc, 'aberta')"));
    q.bindValue(QStringLiteral(":desc"), descricao.trimmed());
    q.bindValue(QStringLiteral(":valor"), valor);
    q.bindValue(QStringLiteral(":venc"), vencimento.isEmpty() ? QVariant() : QVariant(vencimento));
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

ResumoFinanceiro FinanceiroRepository::resumo()
{
    ResumoFinanceiro r;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(valor),0) FROM contas_pagar WHERE status='aberta'"))
        && q.next())
        r.totalAPagar = q.value(0).toLongLong();
    if (q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(valor),0) FROM contas_receber WHERE status='aberta'"))
        && q.next())
        r.totalAReceber = q.value(0).toLongLong();
    return r;
}
