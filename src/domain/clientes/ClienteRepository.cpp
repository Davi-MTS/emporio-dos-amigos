#include "domain/clientes/ClienteRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

ClienteRepository::ClienteRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Cliente> ClienteRepository::listar(const QString &filtro)
{
    QVector<Cliente> lista;
    QString sql = QStringLiteral(
        "SELECT c.id, c.nome, c.telefone, c.cpf, c.endereco, c.aniversario, "
        "       c.observacoes, c.limite_fiado, "
        "       COALESCE((SELECT SUM(valor) FROM contas_receber cr "
        "                 WHERE cr.cliente_id = c.id AND cr.status = 'aberta'), 0) "
        "FROM clientes c WHERE c.ativo = 1 ");
    const QString f = filtro.trimmed();
    if (!f.isEmpty())
        sql += QStringLiteral("AND (c.nome LIKE :like OR c.telefone LIKE :like OR c.cpf LIKE :like) ");
    sql += QStringLiteral("ORDER BY c.nome COLLATE NOCASE");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (!f.isEmpty())
        q.bindValue(QStringLiteral(":like"), QStringLiteral("%") + f + QStringLiteral("%"));
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Cliente c;
        c.id = q.value(0).toInt();
        c.nome = q.value(1).toString();
        c.telefone = q.value(2).toString();
        c.cpf = q.value(3).toString();
        c.endereco = q.value(4).toString();
        c.aniversario = q.value(5).toString();
        c.observacoes = q.value(6).toString();
        c.limiteFiado = q.value(7).toLongLong();
        c.saldoDevedor = q.value(8).toLongLong();
        lista.push_back(c);
    }
    return lista;
}

std::optional<Cliente> ClienteRepository::obter(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, nome, telefone, cpf, endereco, aniversario, observacoes, limite_fiado "
        "FROM clientes WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    Cliente c;
    c.id = q.value(0).toInt();
    c.nome = q.value(1).toString();
    c.telefone = q.value(2).toString();
    c.cpf = q.value(3).toString();
    c.endereco = q.value(4).toString();
    c.aniversario = q.value(5).toString();
    c.observacoes = q.value(6).toString();
    c.limiteFiado = q.value(7).toLongLong();
    c.saldoDevedor = saldoDevedor(id);
    return c;
}

bool ClienteRepository::salvar(Cliente &cliente)
{
    if (cliente.nome.trimmed().isEmpty()) {
        m_erro = QStringLiteral("O nome do cliente é obrigatório.");
        return false;
    }
    QSqlQuery q(m_db);
    if (cliente.id == 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO clientes (nome, telefone, cpf, endereco, aniversario, "
            "observacoes, limite_fiado, ativo) "
            "VALUES (:nome, :tel, :cpf, :end, :aniv, :obs, :limite, 1)"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE clientes SET nome=:nome, telefone=:tel, cpf=:cpf, endereco=:end, "
            "aniversario=:aniv, observacoes=:obs, limite_fiado=:limite WHERE id=:id"));
        q.bindValue(QStringLiteral(":id"), cliente.id);
    }
    q.bindValue(QStringLiteral(":nome"), cliente.nome.trimmed());
    q.bindValue(QStringLiteral(":tel"), cliente.telefone);
    q.bindValue(QStringLiteral(":cpf"), cliente.cpf);
    q.bindValue(QStringLiteral(":end"), cliente.endereco);
    q.bindValue(QStringLiteral(":aniv"), cliente.aniversario.isEmpty() ? QVariant() : QVariant(cliente.aniversario));
    q.bindValue(QStringLiteral(":obs"), cliente.observacoes);
    q.bindValue(QStringLiteral(":limite"), cliente.limiteFiado);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    if (cliente.id == 0)
        cliente.id = q.lastInsertId().toInt();
    return true;
}

bool ClienteRepository::inativar(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE clientes SET ativo = 0 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

qint64 ClienteRepository::saldoDevedor(int clienteId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(valor), 0) FROM contas_receber "
        "WHERE cliente_id = :id AND status = 'aberta'"));
    q.bindValue(QStringLiteral(":id"), clienteId);
    if (q.exec() && q.next())
        return q.value(0).toLongLong();
    return 0;
}

qint64 ClienteRepository::quitar(int clienteId)
{
    const qint64 total = saldoDevedor(clienteId);
    if (total <= 0)
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE contas_receber SET status = 'paga', pago_em = date('now','localtime') "
        "WHERE cliente_id = :id AND status = 'aberta'"));
    q.bindValue(QStringLiteral(":id"), clienteId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return total;
}

qint64 ClienteRepository::aplicarRecebimento(int clienteId, qint64 valor)
{
    if (valor <= 0)
        return 0;

    // Contas abertas, da mais antiga para a mais nova (vencimento, depois id).
    struct Conta { int id; qint64 valor; };
    QVector<Conta> contas;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT id, valor FROM contas_receber "
            "WHERE cliente_id = :id AND status = 'aberta' "
            "ORDER BY vencimento IS NULL, vencimento ASC, id ASC"));
        q.bindValue(QStringLiteral(":id"), clienteId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            return 0;
        }
        while (q.next())
            contas.push_back({q.value(0).toInt(), q.value(1).toLongLong()});
    }

    qint64 restante = valor;
    qint64 aplicado = 0;
    for (const Conta &c : contas) {
        if (restante <= 0)
            break;
        if (restante >= c.valor) {
            // Cobre a conta inteira: marca como paga.
            QSqlQuery u(m_db);
            u.prepare(QStringLiteral(
                "UPDATE contas_receber SET status = 'paga', pago_em = date('now','localtime') "
                "WHERE id = :id"));
            u.bindValue(QStringLiteral(":id"), c.id);
            if (!u.exec()) {
                m_erro = u.lastError().text();
                return aplicado;
            }
            restante -= c.valor;
            aplicado += c.valor;
        } else {
            // Pagamento parcial: reduz o saldo desta conta (continua aberta).
            QSqlQuery u(m_db);
            u.prepare(QStringLiteral(
                "UPDATE contas_receber SET valor = valor - :p WHERE id = :id"));
            u.bindValue(QStringLiteral(":p"), restante);
            u.bindValue(QStringLiteral(":id"), c.id);
            if (!u.exec()) {
                m_erro = u.lastError().text();
                return aplicado;
            }
            aplicado += restante;
            restante = 0;
        }
    }
    return aplicado;
}

ClienteRepository::ResumoFiado ClienteRepository::resumoFiado()
{
    ResumoFiado r;
    QSqlQuery q(m_db);

    // Uma consulta só: por cliente, quanto deve e quanto está vencido.
    if (!q.exec(QStringLiteral(
            "SELECT c.nome, c.limite_fiado, "
            "       SUM(cr.valor) AS deve, "
            "       SUM(CASE WHEN cr.vencimento IS NOT NULL "
            "                 AND cr.vencimento < date('now','localtime') "
            "                THEN cr.valor ELSE 0 END) AS vencido "
            "FROM contas_receber cr "
            "JOIN clientes c ON c.id = cr.cliente_id "
            "WHERE cr.status = 'aberta' "
            "GROUP BY cr.cliente_id "
            "HAVING deve > 0 "
            "ORDER BY deve DESC"))) {
        m_erro = q.lastError().text();
        return r;
    }

    while (q.next()) {
        const QString nome = q.value(0).toString();
        const qint64 limite = q.value(1).toLongLong();
        const qint64 deve = q.value(2).toLongLong();
        const qint64 vencido = q.value(3).toLongLong();

        r.total += deve;
        r.atrasado += vencido;
        r.quantosDevem++;
        if (vencido > 0)
            r.quantosAtrasados++;
        // Limite 0 quer dizer "sem limite combinado" — não conta como estouro.
        if (limite > 0 && deve > limite)
            r.acimaDoLimite++;
        if (deve > r.maiorDevedorValor) {
            r.maiorDevedorValor = deve;
            r.maiorDevedorNome = nome;
        }
    }
    m_erro.clear();
    return r;
}

ClienteRepository::HistoricoFiado ClienteRepository::historicoFiado(int clienteId)
{
    HistoricoFiado h;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT MAX(v.data_hora), "
        "       (SELECT MAX(pago_em) FROM contas_receber "
        "         WHERE cliente_id = :c1 AND pago_em IS NOT NULL), "
        "       (SELECT COUNT(*) FROM contas_receber "
        "         WHERE cliente_id = :c2 AND status = 'aberta'), "
        "       (SELECT MIN(vencimento) FROM contas_receber "
        "         WHERE cliente_id = :c3 AND status = 'aberta' AND vencimento IS NOT NULL) "
        "FROM contas_receber cr "
        "LEFT JOIN vendas v ON v.id = cr.venda_id "
        "WHERE cr.cliente_id = :c4"));
    q.bindValue(QStringLiteral(":c1"), clienteId);
    q.bindValue(QStringLiteral(":c2"), clienteId);
    q.bindValue(QStringLiteral(":c3"), clienteId);
    q.bindValue(QStringLiteral(":c4"), clienteId);
    if (!q.exec() || !q.next()) {
        m_erro = q.lastError().text();
        return h;
    }
    h.ultimaCompra = q.value(0).toString();
    h.ultimoPagamento = q.value(1).toString();
    h.contasAbertas = q.value(2).toInt();
    h.vencimentoMaisAntigo = q.value(3).toString();
    m_erro.clear();
    return h;
}
