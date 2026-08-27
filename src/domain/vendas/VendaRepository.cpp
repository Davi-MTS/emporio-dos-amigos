#include "domain/vendas/VendaRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

VendaRepository::VendaRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

ResultadoVenda VendaRepository::registrarVenda(int sessaoId, int clienteId,
                                               qint64 descontoGeral,
                                               const QVector<LinhaVenda> &itens,
                                               const QVector<PagamentoVenda> &pagamentos,
                                               int usuarioId)
{
    ResultadoVenda res;

    if (sessaoId <= 0) {
        res.erro = QStringLiteral("Nenhum caixa aberto.");
        return res;
    }
    if (itens.isEmpty()) {
        res.erro = QStringLiteral("A venda não tem itens.");
        return res;
    }

    // Total = soma dos subtotais - desconto geral.
    qint64 total = 0;
    for (const LinhaVenda &l : itens)
        total += l.qtdEmbalagem * l.precoUnit - l.desconto;
    total -= descontoGeral;
    if (total < 0)
        total = 0;

    qint64 pago = 0;
    qint64 pagoDinheiro = 0;
    bool temFiado = false;
    for (const PagamentoVenda &p : pagamentos) {
        pago += p.valor;
        if (p.forma == QStringLiteral("dinheiro"))
            pagoDinheiro += p.valor;
        if (p.forma == QStringLiteral("fiado"))
            temFiado = true;
    }
    if (temFiado && clienteId <= 0) {
        res.erro = QStringLiteral("Venda no fiado exige um cliente.");
        return res;
    }
    if (temFiado) {
        qint64 fiadoTotal = 0;
        for (const PagamentoVenda &p : pagamentos)
            if (p.forma == QStringLiteral("fiado"))
                fiadoTotal += p.valor;

        qint64 limite = 0;
        qint64 saldo = 0;
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral("SELECT limite_fiado FROM clientes WHERE id = :id"));
            q.bindValue(QStringLiteral(":id"), clienteId);
            if (q.exec() && q.next())
                limite = q.value(0).toLongLong();
        }
        {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral(
                "SELECT COALESCE(SUM(valor), 0) FROM contas_receber "
                "WHERE cliente_id = :id AND status = 'aberta'"));
            q.bindValue(QStringLiteral(":id"), clienteId);
            if (q.exec() && q.next())
                saldo = q.value(0).toLongLong();
        }
        if (limite <= 0) {
            res.erro = QStringLiteral("Cliente sem limite de fiado.");
            return res;
        }
        if (saldo + fiadoTotal > limite) {
            res.erro = QStringLiteral("Limite de fiado excedido.");
            return res;
        }
    }
    if (pago < total) {
        res.erro = QStringLiteral("Pagamento insuficiente.");
        return res;
    }

    res.total = total;
    // Troco só existe sobre DINHEIRO: é o excedente devolvido da gaveta. Excesso
    // em pix/cartão/fiado não vira troco (senão o fechamento tira dinheiro que
    // nunca entrou — já causou `valor_esperado` negativo).
    res.troco = qBound(Q_INT64_C(0), pago - total, pagoDinheiro);

    if (!m_db.transaction()) {
        res.erro = m_db.lastError().text();
        return res;
    }

    // Venda
    int vendaId = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            // data em hora LOCAL (o default do SQLite seria UTC e jogaria as
            // vendas da noite para o dia seguinte nos relatórios).
            "INSERT INTO vendas (sessao_id, cliente_id, total, desconto, troco, status, usuario_id, data) "
            "VALUES (:sid, :cid, :total, :desc, :troco, 'concluida', :uid, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        q.bindValue(QStringLiteral(":cid"), clienteId > 0 ? QVariant(clienteId) : QVariant());
        q.bindValue(QStringLiteral(":total"), total);
        q.bindValue(QStringLiteral(":desc"), descontoGeral);
        q.bindValue(QStringLiteral(":troco"), res.troco);
        q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        if (!q.exec()) {
            res.erro = q.lastError().text();
            m_db.rollback();
            return res;
        }
        vendaId = q.lastInsertId().toInt();
    }

    // Itens + baixa de estoque + movimentações
    for (const LinhaVenda &l : itens) {
        const qint64 qtdBase = l.qtdEmbalagem * (l.fator > 0 ? l.fator : 1);

        QSqlQuery qi(m_db);
        qi.prepare(QStringLiteral(
            "INSERT INTO venda_itens "
            "(venda_id, produto_id, embalagem_id, qtd_unidade_base, preco_unit, desconto) "
            "VALUES (:vid, :pid, :eid, :qtd, :preco, :desc)"));
        qi.bindValue(QStringLiteral(":vid"), vendaId);
        qi.bindValue(QStringLiteral(":pid"), l.produtoId);
        qi.bindValue(QStringLiteral(":eid"), l.embalagemId > 0 ? QVariant(l.embalagemId) : QVariant());
        qi.bindValue(QStringLiteral(":qtd"), qtdBase);
        qi.bindValue(QStringLiteral(":preco"), l.precoUnit);
        qi.bindValue(QStringLiteral(":desc"), l.desconto);
        if (!qi.exec()) {
            res.erro = qi.lastError().text();
            m_db.rollback();
            return res;
        }

        // Produto composto ("copão")? baixa os insumos; normal baixa a si mesmo.
        bool composto = false;
        {
            QSqlQuery qc(m_db);
            qc.prepare(QStringLiteral("SELECT composto FROM produtos WHERE id = :pid"));
            qc.bindValue(QStringLiteral(":pid"), l.produtoId);
            if (qc.exec() && qc.next())
                composto = qc.value(0).toInt() != 0;
        }

        struct Baixa { int produtoId; qint64 qtd; };
        QVector<Baixa> baixas;
        if (composto) {
            // Os insumos já vêm resolvidos da venda (o produto específico foi
            // escolhido na hora). Cada um baixa qtdBase * quantidade_da_receita.
            if (l.insumos.isEmpty()) {
                res.erro = QStringLiteral("Escolha os insumos do produto composto.");
                m_db.rollback();
                return res;
            }
            for (const InsumoResolvido &ins : l.insumos)
                baixas.push_back({ins.produtoId, qtdBase * ins.quantidade});
        } else {
            baixas.push_back({l.produtoId, qtdBase});
        }

        for (const Baixa &b : baixas) {
            QSqlQuery qe1(m_db);
            qe1.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO estoque (produto_id, quantidade_atual, custo_medio_unitario) "
                "VALUES (:pid, 0, 0)"));
            qe1.bindValue(QStringLiteral(":pid"), b.produtoId);
            if (!qe1.exec()) {
                res.erro = qe1.lastError().text();
                m_db.rollback();
                return res;
            }

            // Custo do produto NO MOMENTO da venda (a venda não altera o custo
            // médio, então lê-lo agora capta o COGS correto). Gravado na
            // movimentação para o lucro não mudar com compras futuras.
            qint64 custoUnit = 0;
            {
                QSqlQuery qc(m_db);
                qc.prepare(QStringLiteral(
                    "SELECT custo_medio_unitario FROM estoque WHERE produto_id = :pid"));
                qc.bindValue(QStringLiteral(":pid"), b.produtoId);
                if (qc.exec() && qc.next())
                    custoUnit = qc.value(0).toLongLong();
            }

            QSqlQuery qe2(m_db);
            qe2.prepare(QStringLiteral(
                "UPDATE estoque SET quantidade_atual = quantidade_atual - :qtd WHERE produto_id = :pid"));
            qe2.bindValue(QStringLiteral(":qtd"), b.qtd);
            qe2.bindValue(QStringLiteral(":pid"), b.produtoId);
            if (!qe2.exec()) {
                res.erro = qe2.lastError().text();
                m_db.rollback();
                return res;
            }

            QSqlQuery qm(m_db);
            qm.prepare(QStringLiteral(
                "INSERT INTO movimentacoes_estoque "
                "(produto_id, tipo, quantidade, origem, usuario_id, custo_unit, data) "
                "VALUES (:pid, 'saida_venda', :qtd, :origem, :uid, :custo, datetime('now','localtime'))"));
            qm.bindValue(QStringLiteral(":pid"), b.produtoId);
            qm.bindValue(QStringLiteral(":qtd"), -b.qtd); // saída = negativo
            qm.bindValue(QStringLiteral(":origem"), QStringLiteral("venda:%1").arg(vendaId));
            qm.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
            qm.bindValue(QStringLiteral(":custo"), custoUnit);
            if (!qm.exec()) {
                res.erro = qm.lastError().text();
                m_db.rollback();
                return res;
            }
        }
    }

    // Pagamentos (+ conta a receber no fiado)
    for (const PagamentoVenda &p : pagamentos) {
        QSqlQuery qp(m_db);
        qp.prepare(QStringLiteral(
            "INSERT INTO pagamentos (venda_id, forma, valor, status) "
            "VALUES (:vid, :forma, :valor, 'aprovado')"));
        qp.bindValue(QStringLiteral(":vid"), vendaId);
        qp.bindValue(QStringLiteral(":forma"), p.forma);
        qp.bindValue(QStringLiteral(":valor"), p.valor);
        if (!qp.exec()) {
            res.erro = qp.lastError().text();
            m_db.rollback();
            return res;
        }

        if (p.forma == QStringLiteral("fiado")) {
            QSqlQuery qr(m_db);
            qr.prepare(QStringLiteral(
                "INSERT INTO contas_receber (cliente_id, venda_id, valor, status) "
                "VALUES (:cid, :vid, :valor, 'aberta')"));
            qr.bindValue(QStringLiteral(":cid"), clienteId);
            qr.bindValue(QStringLiteral(":vid"), vendaId);
            qr.bindValue(QStringLiteral(":valor"), p.valor);
            if (!qr.exec()) {
                res.erro = qr.lastError().text();
                m_db.rollback();
                return res;
            }
        }
    }

    if (!m_db.commit()) {
        res.erro = m_db.lastError().text();
        m_db.rollback();
        return res;
    }

    res.ok = true;
    res.vendaId = vendaId;
    return res;
}
