#include "domain/lotes/LoteRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <utility>

LoteRepository::LoteRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

bool LoteRepository::registrar(int produtoId, qint64 quantidadeBase,
                               const QString &validade, const QString &codigo)
{
    if (produtoId <= 0 || quantidadeBase <= 0 || validade.trimmed().isEmpty()) {
        m_erro = QStringLiteral("Lote precisa de produto, quantidade e validade.");
        return false;
    }

    // Mesma validade e mesmo código: soma no lote existente em vez de criar
    // linhas repetidas a cada carga da semana.
    QSqlQuery busca(m_db);
    busca.prepare(QStringLiteral(
        "SELECT id FROM lotes WHERE produto_id = :pid AND data_validade = :val "
        "  AND COALESCE(codigo_lote, '') = :cod"));
    busca.bindValue(QStringLiteral(":pid"), produtoId);
    busca.bindValue(QStringLiteral(":val"), validade);
    busca.bindValue(QStringLiteral(":cod"), codigo.trimmed());
    if (busca.exec() && busca.next()) {
        QSqlQuery upd(m_db);
        upd.prepare(QStringLiteral(
            "UPDATE lotes SET quantidade = quantidade + :q WHERE id = :id"));
        upd.bindValue(QStringLiteral(":q"), quantidadeBase);
        upd.bindValue(QStringLiteral(":id"), busca.value(0).toInt());
        if (!upd.exec()) {
            m_erro = upd.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral(
        "INSERT INTO lotes (produto_id, codigo_lote, data_validade, quantidade) "
        "VALUES (:pid, :cod, :val, :q)"));
    ins.bindValue(QStringLiteral(":pid"), produtoId);
    ins.bindValue(QStringLiteral(":cod"),
                  codigo.trimmed().isEmpty() ? QVariant() : QVariant(codigo.trimmed()));
    ins.bindValue(QStringLiteral(":val"), validade);
    ins.bindValue(QStringLiteral(":q"), quantidadeBase);
    if (!ins.exec()) {
        m_erro = ins.lastError().text();
        return false;
    }
    return true;
}

qint64 LoteRepository::consumirFefo(int produtoId, qint64 quantidadeBase)
{
    if (produtoId <= 0 || quantidadeBase <= 0)
        return 0;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, quantidade FROM lotes "
        "WHERE produto_id = :pid AND quantidade > 0 "
        "ORDER BY data_validade IS NULL, data_validade ASC, id ASC"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }

    QVector<QPair<int, qint64>> lotes;
    while (q.next())
        lotes.push_back({q.value(0).toInt(), q.value(1).toLongLong()});

    qint64 restante = quantidadeBase;
    qint64 consumido = 0;
    for (const auto &lote : lotes) {
        if (restante <= 0)
            break;
        const qint64 tira = qMin(restante, lote.second);

        QSqlQuery upd(m_db);
        upd.prepare(QStringLiteral(
            "UPDATE lotes SET quantidade = quantidade - :q WHERE id = :id"));
        upd.bindValue(QStringLiteral(":q"), tira);
        upd.bindValue(QStringLiteral(":id"), lote.first);
        if (!upd.exec()) {
            m_erro = upd.lastError().text();
            return consumido;
        }
        restante -= tira;
        consumido += tira;
    }

    // Lote zerado não interessa mais a ninguém e sujaria a tela de validades.
    QSqlQuery limpa(m_db);
    limpa.prepare(QStringLiteral("DELETE FROM lotes WHERE produto_id = :pid AND quantidade <= 0"));
    limpa.bindValue(QStringLiteral(":pid"), produtoId);
    limpa.exec();

    return consumido;
}

QVector<Lote> LoteRepository::listar(int diasLimite)
{
    QVector<Lote> lista;

    QString sql = QStringLiteral(
        "SELECT l.id, l.produto_id, p.nome, p.unidade_base, "
        "       COALESCE(l.codigo_lote, ''), l.data_validade, l.quantidade, "
        "       CAST(julianday(l.data_validade) - julianday(date('now','localtime')) AS INTEGER) "
        "FROM lotes l "
        "JOIN produtos p ON p.id = l.produto_id "
        "WHERE l.quantidade > 0 AND l.data_validade IS NOT NULL ");
    if (diasLimite >= 0)
        sql += QStringLiteral("  AND l.data_validade <= date('now','localtime','+' || :dias || ' day') ");
    sql += QStringLiteral("ORDER BY l.data_validade ASC, p.nome COLLATE NOCASE ASC");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (diasLimite >= 0)
        q.bindValue(QStringLiteral(":dias"), diasLimite);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Lote l;
        l.id = q.value(0).toInt();
        l.produtoId = q.value(1).toInt();
        l.produtoNome = q.value(2).toString();
        l.unidadeBase = q.value(3).toString();
        l.codigo = q.value(4).toString();
        l.validade = q.value(5).toString();
        l.quantidade = q.value(6).toLongLong();
        l.diasParaVencer = q.value(7).toInt();
        lista.push_back(l);
    }
    return lista;
}

ResumoVencimento LoteRepository::resumo()
{
    ResumoVencimento r;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT "
            "  SUM(CASE WHEN data_validade < date('now','localtime') THEN 1 ELSE 0 END), "
            "  SUM(CASE WHEN data_validade < date('now','localtime') THEN quantidade ELSE 0 END), "
            "  SUM(CASE WHEN data_validade >= date('now','localtime') "
            "            AND data_validade <= date('now','localtime','+7 day') THEN 1 ELSE 0 END), "
            "  SUM(CASE WHEN data_validade >= date('now','localtime') "
            "            AND data_validade <= date('now','localtime','+30 day') THEN 1 ELSE 0 END) "
            "FROM lotes WHERE quantidade > 0 AND data_validade IS NOT NULL"))) {
        m_erro = q.lastError().text();
        return r;
    }
    if (q.next()) {
        r.vencidos = q.value(0).toInt();
        r.quantidadeVencida = q.value(1).toLongLong();
        r.venceEm7 = q.value(2).toInt();
        r.venceEm30 = q.value(3).toInt();
    }
    return r;
}

qint64 LoteRepository::totalEmLotes(int produtoId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(quantidade), 0) FROM lotes WHERE produto_id = :pid"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (!q.exec() || !q.next())
        return 0;
    return q.value(0).toLongLong();
}

QVector<QPair<QString, qint64>> LoteRepository::divergencias()
{
    QVector<QPair<QString, qint64>> lista;
    QSqlQuery q(m_db);
    // Só produtos que TÊM algum lote: quem nunca teve validade informada não é
    // divergência, é apenas produto sem controle de vencimento.
    if (!q.exec(QStringLiteral(
            "SELECT p.nome, COALESCE(e.quantidade_atual, 0) - SUM(l.quantidade) "
            "FROM lotes l "
            "JOIN produtos p ON p.id = l.produto_id "
            "LEFT JOIN estoque e ON e.produto_id = l.produto_id "
            "WHERE l.quantidade > 0 "
            "GROUP BY l.produto_id "
            "HAVING COALESCE(e.quantidade_atual, 0) <> SUM(l.quantidade) "
            "ORDER BY p.nome COLLATE NOCASE"))) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next())
        lista.push_back({q.value(0).toString(), q.value(1).toLongLong()});
    return lista;
}
