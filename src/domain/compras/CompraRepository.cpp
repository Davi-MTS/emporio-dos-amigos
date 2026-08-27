#include "domain/compras/CompraRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

CompraRepository::CompraRepository(QSqlDatabase db)
    : m_db(std::move(db))
    , m_estoque(m_db)
{
}

QVector<CompraResumo> CompraRepository::listar()
{
    QVector<CompraResumo> lista;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT c.id, COALESCE(f.nome, '—'), c.data, c.total, c.status, "
            "       (SELECT COUNT(*) FROM compra_itens ci WHERE ci.compra_id = c.id), "
            "       COALESCE(c.numero_nota, '') "
            "FROM compras c LEFT JOIN fornecedores f ON f.id = c.fornecedor_id "
            "ORDER BY c.id DESC"))) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        CompraResumo c;
        c.id = q.value(0).toInt();
        c.fornecedorNome = q.value(1).toString();
        c.data = q.value(2).toString();
        c.total = q.value(3).toLongLong();
        c.status = q.value(4).toString();
        c.numItens = q.value(5).toInt();
        c.numeroNota = q.value(6).toString();
        lista.push_back(c);
    }
    return lista;
}

ResultadoCompra CompraRepository::registrarCompra(int fornecedorId,
                                                  const QString &origem,
                                                  const QVector<ItemCompra> &itens,
                                                  bool gerarContaPagar,
                                                  const QString &vencimento,
                                                  int usuarioId,
                                                  const QString &numeroNota,
                                                  const QString &dataNota)
{
    ResultadoCompra res;
    if (itens.isEmpty()) {
        res.erro = QStringLiteral("A compra não tem itens.");
        return res;
    }

    qint64 total = 0;
    for (const ItemCompra &it : itens)
        total += it.qtdEmbalagem * it.custoUnitEmbalagem;
    res.total = total;

    if (!m_db.transaction()) {
        res.erro = m_db.lastError().text();
        return res;
    }

    const QString numNota = numeroNota.trimmed();
    const QString origemFinal = !numNota.isEmpty() ? QStringLiteral("nota")
                                : (origem.isEmpty() ? QStringLiteral("manual") : origem);

    int compraId = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO compras (fornecedor_id, total, status, origem, numero_nota, data_nota, data) "
            "VALUES (:forn, :total, 'recebida', :origem, :numnota, :datanota, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":forn"), fornecedorId > 0 ? QVariant(fornecedorId) : QVariant());
        q.bindValue(QStringLiteral(":total"), total);
        q.bindValue(QStringLiteral(":origem"), origemFinal);
        q.bindValue(QStringLiteral(":numnota"), numNota.isEmpty() ? QVariant() : QVariant(numNota));
        q.bindValue(QStringLiteral(":datanota"), dataNota.isEmpty() ? QVariant() : QVariant(dataNota));
        if (!q.exec()) {
            res.erro = q.lastError().text();
            m_db.rollback();
            return res;
        }
        compraId = q.lastInsertId().toInt();
    }

    for (const ItemCompra &it : itens) {
        const int fator = it.fator > 0 ? it.fator : 1;
        const qint64 qtdBase = it.qtdEmbalagem * fator;
        // Custo por unidade base em MILÉSIMOS de centavo: multiplica por 1000
        // ANTES de dividir pelo fator, para não perder a fração (ex.: ml).
        const qint64 custoUnitBase = it.custoUnitEmbalagem * 1000 / fator;

        QSqlQuery qi(m_db);
        qi.prepare(QStringLiteral(
            "INSERT INTO compra_itens "
            "(compra_id, produto_id, embalagem_id, qtd_unidade_base, custo_unit) "
            "VALUES (:cid, :pid, :eid, :qtd, :custo)"));
        qi.bindValue(QStringLiteral(":cid"), compraId);
        qi.bindValue(QStringLiteral(":pid"), it.produtoId);
        qi.bindValue(QStringLiteral(":eid"), it.embalagemId > 0 ? QVariant(it.embalagemId) : QVariant());
        qi.bindValue(QStringLiteral(":qtd"), qtdBase);
        qi.bindValue(QStringLiteral(":custo"), custoUnitBase);
        if (!qi.exec()) {
            res.erro = qi.lastError().text();
            m_db.rollback();
            return res;
        }

        // Entrada no estoque (custo médio) dentro desta mesma transação.
        if (!m_estoque.aplicarEntrada(it.produtoId, qtdBase, custoUnitBase, usuarioId,
                                      QStringLiteral("compra:%1").arg(compraId), QString())) {
            res.erro = m_estoque.ultimoErro();
            m_db.rollback();
            return res;
        }
    }

    if (gerarContaPagar) {
        QSqlQuery qc(m_db);
        qc.prepare(QStringLiteral(
            "INSERT INTO contas_pagar (compra_id, descricao, valor, vencimento, status) "
            "VALUES (:cid, :desc, :valor, :venc, 'aberta')"));
        qc.bindValue(QStringLiteral(":cid"), compraId);
        qc.bindValue(QStringLiteral(":desc"),
                     numNota.isEmpty() ? QStringLiteral("Compra #%1").arg(compraId)
                                       : QStringLiteral("NF %1").arg(numNota));
        qc.bindValue(QStringLiteral(":valor"), total);
        qc.bindValue(QStringLiteral(":venc"), vencimento.isEmpty() ? QVariant() : QVariant(vencimento));
        if (!qc.exec()) {
            res.erro = qc.lastError().text();
            m_db.rollback();
            return res;
        }
    }

    if (!m_db.commit()) {
        res.erro = m_db.lastError().text();
        m_db.rollback();
        return res;
    }

    res.ok = true;
    res.compraId = compraId;
    return res;
}
