#include "domain/vendas/VendaRepository.h"

#include "domain/lotes/LoteRepository.h"

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
        res.erro = QStringLiteral("Nenhum caixa aberto");
        return res;
    }
    if (itens.isEmpty()) {
        res.erro = QStringLiteral("Venda sem itens");
        return res;
    }
    // Quantidade zero ou negativa DEVOLVERIA mercadoria ao estoque como se
    // fosse venda; valor negativo abateria o total. A tela não deixa, mas a
    // regra que protege o estoque e o caixa fica aqui.
    for (const LinhaVenda &l : itens) {
        if (l.qtdEmbalagem <= 0 || l.precoUnit < 0 || l.desconto < 0) {
            res.erro = QStringLiteral("Quantidade ou valor inválido");
            return res;
        }
    }
    for (const PagamentoVenda &p : pagamentos) {
        if (p.valor <= 0) {
            res.erro = QStringLiteral("Pagamento inválido");
            return res;
        }
    }
    if (descontoGeral < 0) {
        res.erro = QStringLiteral("Desconto inválido");
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
        res.erro = QStringLiteral("Fiado exige cliente");
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
            res.erro = QStringLiteral("Cliente sem limite de fiado");
            return res;
        }
        if (saldo + fiadoTotal > limite) {
            res.erro = QStringLiteral("Limite de fiado excedido");
            return res;
        }
    }
    if (pago < total) {
        res.erro = QStringLiteral("Pagamento insuficiente");
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
        if (composto && l.insumos.isEmpty()) {
            // Composto sem receita resolvida é erro: não dá para adivinhar qual
            // produto sai do estoque.
            res.erro = QStringLiteral("Escolha os insumos");
            m_db.rollback();
            return res;
        }
        if (!l.insumos.isEmpty()) {
            // Vale para o composto (insumos escolhidos na hora) e para a DOSE,
            // cuja origem é sempre a mesma garrafa e vem resolvida pelo backend.
            // Cada um baixa qtdBase * quantidade da receita.
            for (const InsumoResolvido &ins : l.insumos)
                baixas.push_back({ins.produtoId, qtdBase * ins.quantidade});
        } else {
            baixas.push_back({l.produtoId, qtdBase});
        }

        // O que sai da prateleira sai de uma remessa: consome o lote que vence
        // primeiro (FEFO). Best effort — parte do estoque pode ter entrado sem
        // validade informada, e aí simplesmente não há lote para baixar.
        LoteRepository lotes(m_db);
        for (const Baixa &b : baixas)
            lotes.consumirFefo(b.produtoId, b.qtd);

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
            qint64 saldoAntes = 0;
            {
                QSqlQuery qc(m_db);
                qc.prepare(QStringLiteral(
                    "SELECT custo_medio_unitario, quantidade_atual FROM estoque WHERE produto_id = :pid"));
                qc.bindValue(QStringLiteral(":pid"), b.produtoId);
                if (qc.exec() && qc.next()) {
                    custoUnit = qc.value(0).toLongLong();
                    saldoAntes = qc.value(1).toLongLong();
                }
            }
            // Unidades vendidas além do que havia no estoque. O custo delas ainda
            // não existe (a mercadoria não foi lançada): ficam com o custo médio
            // de agora como PROVISÓRIO e são acertadas quando a compra chegar —
            // ver EstoqueRepository::acertarCustoPendente.
            const qint64 pendente = qBound(Q_INT64_C(0), b.qtd - qMax(Q_INT64_C(0), saldoAntes), b.qtd);

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
                "(produto_id, tipo, quantidade, origem, usuario_id, custo_unit, qtd_pendente_custo, data) "
                "VALUES (:pid, 'saida_venda', :qtd, :origem, :uid, :custo, :pend, datetime('now','localtime'))"));
            qm.bindValue(QStringLiteral(":pid"), b.produtoId);
            qm.bindValue(QStringLiteral(":qtd"), -b.qtd); // saída = negativo
            qm.bindValue(QStringLiteral(":origem"), QStringLiteral("venda:%1").arg(vendaId));
            qm.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
            qm.bindValue(QStringLiteral(":custo"), custoUnit);
            qm.bindValue(QStringLiteral(":pend"), pendente);
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

bool VendaRepository::cancelarVenda(int vendaId, const QString &motivo, int usuarioId,
                                    int sessaoAbertaId)
{
    m_aviso.clear();
    // Confere se a venda existe e ainda pode ser cancelada.
    int sessaoDaVenda = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT status, sessao_id FROM vendas WHERE id = :id"));
        q.bindValue(QStringLiteral(":id"), vendaId);
        if (!q.exec() || !q.next()) {
            m_erro = QStringLiteral("Venda não encontrada");
            return false;
        }
        if (q.value(0).toString() != QStringLiteral("concluida")) {
            m_erro = QStringLiteral("Venda já cancelada");
            return false;
        }
        sessaoDaVenda = q.value(1).toInt();
    }

    // Quanto dinheiro precisa SAIR DA GAVETA para devolver ao cliente:
    //  - o que ele pagou em dinheiro, se a venda é de outro turno (no mesmo
    //    turno o esperado já cai sozinho, porque o resumo só conta vendas
    //    concluídas);
    //  - o que ele já pagou do FIADO desta venda. Antes o cancelamento só
    //    cancelava a conta ainda aberta: o valor recebido ficava como pago numa
    //    venda que não existe mais, sem devolução nenhuma.
    qint64 dinheiroOutroTurno = 0;
    if (sessaoDaVenda != sessaoAbertaId) {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT COALESCE((SELECT SUM(valor) FROM pagamentos "
            "                 WHERE venda_id = :v1 AND forma = 'dinheiro'), 0) - troco "
            "FROM vendas WHERE id = :v2"));
        q.bindValue(QStringLiteral(":v1"), vendaId);
        q.bindValue(QStringLiteral(":v2"), vendaId);
        if (q.exec() && q.next())
            dinheiroOutroTurno = qMax(Q_INT64_C(0), q.value(0).toLongLong());   // o que ficou na gaveta
    }
    qint64 fiadoJaPago = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT COALESCE((SELECT SUM(valor) FROM pagamentos "
            "                 WHERE venda_id = :v1 AND forma = 'fiado'), 0) "
            "     - COALESCE((SELECT SUM(valor) FROM contas_receber "
            "                 WHERE venda_id = :v2 AND status = 'aberta'), 0)"));
        q.bindValue(QStringLiteral(":v1"), vendaId);
        q.bindValue(QStringLiteral(":v2"), vendaId);
        if (q.exec() && q.next())
            fiadoJaPago = qMax(Q_INT64_C(0), q.value(0).toLongLong());
    }
    const qint64 devolver = dinheiroOutroTurno + fiadoJaPago;
    const auto fmt = [](qint64 c) {
        return QStringLiteral("R$ %1,%2").arg(c / 100).arg(c % 100, 2, 10, QLatin1Char('0'));
    };
    // Com o caixa fechado não há turno onde lançar a saída: o dinheiro sairia
    // da gaveta sem aparecer em conferência nenhuma (e a tela dizia que
    // "estorna no caixa"). Pede para abrir o caixa antes.
    if (devolver > 0 && sessaoAbertaId <= 0) {
        m_erro = QStringLiteral("Abra o caixa para cancelar");
        return false;
    }

    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }

    // 1) Devolve ao estoque exatamente o que saiu (inclui insumos de composto).
    //    Lê das movimentações, que registram o produto real baixado.
    struct Devolucao { int produtoId; qint64 qtd; };
    QVector<Devolucao> devolucoes;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT produto_id, quantidade FROM movimentacoes_estoque "
            "WHERE tipo = 'saida_venda' AND origem = :origem"));
        q.bindValue(QStringLiteral(":origem"), QStringLiteral("venda:%1").arg(vendaId));
        if (!q.exec()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
        while (q.next())
            devolucoes.push_back({q.value(0).toInt(), -q.value(1).toLongLong()}); // saída era negativa
    }

    // Venda cancelada não espera mais custo: sem isto, a próxima compra acertaria
    // o custo de uma venda que não vale, e a que ainda está sem estoque de verdade
    // ficaria com o provisório.
    {
        QSqlQuery qp(m_db);
        qp.prepare(QStringLiteral(
            "UPDATE movimentacoes_estoque SET qtd_pendente_custo = 0 "
            "WHERE tipo = 'saida_venda' AND origem = :origem"));
        qp.bindValue(QStringLiteral(":origem"), QStringLiteral("venda:%1").arg(vendaId));
        if (!qp.exec()) {
            m_erro = qp.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    for (const Devolucao &d : devolucoes) {
        QSqlQuery qe(m_db);
        qe.prepare(QStringLiteral(
            "UPDATE estoque SET quantidade_atual = quantidade_atual + :qtd WHERE produto_id = :pid"));
        qe.bindValue(QStringLiteral(":qtd"), d.qtd);
        qe.bindValue(QStringLiteral(":pid"), d.produtoId);
        if (!qe.exec()) {
            m_erro = qe.lastError().text();
            m_db.rollback();
            return false;
        }

        QSqlQuery qm(m_db);
        qm.prepare(QStringLiteral(
            "INSERT INTO movimentacoes_estoque "
            "(produto_id, tipo, quantidade, origem, usuario_id, observacao, data) "
            "VALUES (:pid, 'devolucao', :qtd, :origem, :uid, :obs, datetime('now','localtime'))"));
        qm.bindValue(QStringLiteral(":pid"), d.produtoId);
        qm.bindValue(QStringLiteral(":qtd"), d.qtd);   // entrada = positivo
        qm.bindValue(QStringLiteral(":origem"), QStringLiteral("cancelamento:%1").arg(vendaId));
        qm.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        qm.bindValue(QStringLiteral(":obs"), motivo);
        if (!qm.exec()) {
            m_erro = qm.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 2) Cancela a conta de fiado gerada — o cliente não deve por venda desfeita.
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE contas_receber SET status = 'cancelada' "
            "WHERE venda_id = :vid AND status = 'aberta'"));
        q.bindValue(QStringLiteral(":vid"), vendaId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 3) Dinheiro devolvido ao cliente sai da gaveta como sangria, no turno
    //    aberto (o cálculo está antes da transação).
    const auto lancarSangria = [&](qint64 valor, const QString &motivoSangria) {
        if (valor <= 0)
            return true;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO mov_caixa (sessao_id, tipo, valor, motivo, usuario_id, data) "
            "VALUES (:sid, 'sangria', :valor, :motivo, :uid, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":sid"), sessaoAbertaId);
        q.bindValue(QStringLiteral(":valor"), valor);
        q.bindValue(QStringLiteral(":motivo"), motivoSangria);
        q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        if (!q.exec()) {
            m_erro = q.lastError().text();
            return false;
        }
        return true;
    };
    if (!lancarSangria(dinheiroOutroTurno, QStringLiteral("Estorno da venda #%1").arg(vendaId))
        || !lancarSangria(fiadoJaPago,
                          QStringLiteral("Devolução do fiado já pago da venda #%1").arg(vendaId))) {
        m_db.rollback();
        return false;
    }

    // 4) Marca a venda (nunca apaga: o histórico precisa mostrar o cancelamento).
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE vendas SET status = 'cancelada', cancelada_em = datetime('now','localtime'), "
            "motivo_cancelamento = :motivo WHERE id = :id"));
        q.bindValue(QStringLiteral(":motivo"), motivo);
        q.bindValue(QStringLiteral(":id"), vendaId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_erro = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    m_erro.clear();
    if (devolver > 0)
        m_aviso = QStringLiteral("Saíram %1 da gaveta").arg(fmt(devolver));
    return true;
}

QVector<VendaResumo> VendaRepository::listar(int dias)
{
    QVector<VendaResumo> lista;
    const QString filtro = dias <= 0
        ? QStringLiteral("date(v.data) = date('now','localtime')")
        : QStringLiteral("v.data >= date('now','localtime','-%1 days')").arg(dias - 1);

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT v.id, v.data, COALESCE(c.nome, 'Consumidor final'), v.status, "
            "       v.total, v.troco, COALESCE(v.motivo_cancelamento, ''), "
            "       (SELECT COUNT(*) FROM venda_itens vi WHERE vi.venda_id = v.id), "
            "       (SELECT GROUP_CONCAT(p.forma, ', ') FROM pagamentos p WHERE p.venda_id = v.id) "
            "FROM vendas v LEFT JOIN clientes c ON c.id = v.cliente_id "
            "WHERE %1 ORDER BY v.id DESC").arg(filtro))) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        VendaResumo r;
        r.id = q.value(0).toInt();
        r.data = q.value(1).toString();
        r.clienteNome = q.value(2).toString();
        r.status = q.value(3).toString();
        r.total = q.value(4).toLongLong();
        r.troco = q.value(5).toLongLong();
        r.motivoCancelamento = q.value(6).toString();
        r.numItens = q.value(7).toInt();
        r.formas = q.value(8).toString();
        lista.push_back(r);
    }
    return lista;
}

QVector<ItemVendido> VendaRepository::itens(int vendaId)
{
    QVector<ItemVendido> lista;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT p.nome, COALESCE(pe.nome_embalagem, p.unidade_base), "
        "       vi.qtd_unidade_base, vi.preco_unit, vi.desconto "
        "FROM venda_itens vi "
        "JOIN produtos p ON p.id = vi.produto_id "
        "LEFT JOIN produto_embalagens pe ON pe.id = vi.embalagem_id "
        "WHERE vi.venda_id = :vid ORDER BY vi.id"));
    q.bindValue(QStringLiteral(":vid"), vendaId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        ItemVendido it;
        it.produto = q.value(0).toString();
        it.embalagem = q.value(1).toString();
        it.qtdBase = q.value(2).toLongLong();
        it.precoUnit = q.value(3).toLongLong();
        it.desconto = q.value(4).toLongLong();
        lista.push_back(it);
    }
    return lista;
}
