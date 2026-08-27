#include "domain/caixa/CaixaRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

CaixaRepository::CaixaRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

int CaixaRepository::sessaoAbertaId()
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT id FROM sessoes_caixa WHERE status = 'aberta' ORDER BY id DESC LIMIT 1"))) {
        m_erro = q.lastError().text();
        return 0;
    }
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

int CaixaRepository::abrirSessao(qint64 valorAbertura, int usuarioId)
{
    // Se já houver uma aberta, retorna ela (não abre duas).
    const int existente = sessaoAbertaId();
    if (existente > 0)
        return existente;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO sessoes_caixa (usuario_abertura, valor_abertura, status, aberta_em) "
        "VALUES (:uid, :valor, 'aberta', datetime('now','localtime'))"));
    q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
    q.bindValue(QStringLiteral(":valor"), valorAbertura);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool CaixaRepository::registrarMovimento(int sessaoId, const QString &tipo,
                                         qint64 valor, const QString &motivo,
                                         int usuarioId)
{
    if (tipo != QStringLiteral("sangria") && tipo != QStringLiteral("suprimento")) {
        m_erro = QStringLiteral("Tipo de movimento inválido.");
        return false;
    }
    if (valor <= 0) {
        m_erro = QStringLiteral("O valor deve ser maior que zero.");
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO mov_caixa (sessao_id, tipo, valor, motivo, usuario_id, data) "
        "VALUES (:sid, :tipo, :valor, :motivo, :uid, datetime('now','localtime'))"));
    q.bindValue(QStringLiteral(":sid"), sessaoId);
    q.bindValue(QStringLiteral(":tipo"), tipo);
    q.bindValue(QStringLiteral(":valor"), valor);
    q.bindValue(QStringLiteral(":motivo"), motivo);
    q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool CaixaRepository::registrarRecebimento(int sessaoId, qint64 valor,
                                           const QString &motivo, int usuarioId)
{
    if (sessaoId <= 0 || valor <= 0)
        return false; // sem caixa aberto ou valor nulo: nada a lançar
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO mov_caixa (sessao_id, tipo, valor, motivo, usuario_id, data) "
        "VALUES (:sid, 'recebimento', :valor, :motivo, :uid, datetime('now','localtime'))"));
    q.bindValue(QStringLiteral(":sid"), sessaoId);
    q.bindValue(QStringLiteral(":valor"), valor);
    q.bindValue(QStringLiteral(":motivo"), motivo);
    q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

ResumoCaixa CaixaRepository::resumo(int sessaoId)
{
    ResumoCaixa r;
    r.sessaoId = sessaoId;

    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT valor_abertura FROM sessoes_caixa WHERE id = :sid"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        if (q.exec() && q.next())
            r.abertura = q.value(0).toLongLong();
    }

    // Pagamentos por forma (só vendas concluídas).
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT p.forma, SUM(p.valor) FROM pagamentos p "
            "JOIN vendas v ON v.id = p.venda_id "
            "WHERE v.sessao_id = :sid AND v.status = 'concluida' "
            "GROUP BY p.forma"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        if (q.exec()) {
            while (q.next()) {
                const QString forma = q.value(0).toString();
                const qint64 v = q.value(1).toLongLong();
                if (forma == QStringLiteral("dinheiro")) r.vendasDinheiro = v;
                else if (forma == QStringLiteral("pix")) r.vendasPix = v;
                else if (forma == QStringLiteral("debito")) r.vendasDebito = v;
                else if (forma == QStringLiteral("credito")) r.vendasCredito = v;
                else if (forma == QStringLiteral("fiado")) r.vendasFiado = v;
            }
        }
    }

    // Troco total e número de vendas.
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(troco), 0), COUNT(*), COALESCE(SUM(total), 0) FROM vendas "
            "WHERE sessao_id = :sid AND status = 'concluida'"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        if (q.exec() && q.next()) {
            r.troco = q.value(0).toLongLong();
            r.numVendas = q.value(1).toInt();
            r.totalVendido = q.value(2).toLongLong();
        }
    }

    // Sangrias e suprimentos.
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT tipo, SUM(valor) FROM mov_caixa WHERE sessao_id = :sid GROUP BY tipo"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        if (q.exec()) {
            while (q.next()) {
                const QString tipo = q.value(0).toString();
                const qint64 v = q.value(1).toLongLong();
                if (tipo == QStringLiteral("sangria")) r.sangrias = v;
                else if (tipo == QStringLiteral("suprimento")) r.suprimentos = v;
                else if (tipo == QStringLiteral("recebimento")) r.recebimentos = v;
            }
        }
    }

    return r;
}

ResultadoFechamento CaixaRepository::fechar(int sessaoId, qint64 dinheiroContado,
                                            int usuarioId)
{
    ResultadoFechamento res;

    // Confere se a sessão está aberta.
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT status FROM sessoes_caixa WHERE id = :sid"));
        q.bindValue(QStringLiteral(":sid"), sessaoId);
        if (!q.exec() || !q.next()) {
            res.erro = QStringLiteral("Sessão não encontrada.");
            return res;
        }
        if (q.value(0).toString() != QStringLiteral("aberta")) {
            res.erro = QStringLiteral("A sessão já está fechada.");
            return res;
        }
    }

    const ResumoCaixa r = resumo(sessaoId);
    res.esperado = r.dinheiroEsperado();
    res.informado = dinheiroContado;
    res.diferenca = dinheiroContado - res.esperado;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE sessoes_caixa SET status = 'fechada', fechada_em = datetime('now','localtime'), "
        " usuario_fechamento = :uid, valor_esperado = :esp, valor_informado = :inf, "
        " diferenca = :dif WHERE id = :sid AND status = 'aberta'"));
    q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
    q.bindValue(QStringLiteral(":esp"), res.esperado);
    q.bindValue(QStringLiteral(":inf"), res.informado);
    q.bindValue(QStringLiteral(":dif"), res.diferenca);
    q.bindValue(QStringLiteral(":sid"), sessaoId);
    if (!q.exec()) {
        res.erro = q.lastError().text();
        return res;
    }

    res.ok = true;
    return res;
}
