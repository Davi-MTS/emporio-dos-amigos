#include "domain/estoque/EstoqueRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

EstoqueRepository::EstoqueRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<ItemEstoque> EstoqueRepository::listar(const QString &filtro)
{
    QVector<ItemEstoque> itens;

    QString sql = QStringLiteral(
        "SELECT p.id, p.nome, p.localizacao, p.unidade_base, p.estoque_minimo, "
        "       COALESCE(e.quantidade_atual, 0), COALESCE(e.custo_medio_unitario, 0), "
        "       (p.foto IS NOT NULL) "
        "FROM produtos p "
        "LEFT JOIN estoque e ON e.produto_id = p.id "
        // Composto e dose não têm estoque próprio (baixam insumo/garrafa):
        // listá-los aqui mostraria saldo zero eterno e convidaria a "corrigir"
        // um estoque que não existe.
        "WHERE p.ativo = 1 AND p.composto = 0 "
        "  AND COALESCE(p.dose_de_produto_id, 0) = 0 ");
    const QString f = filtro.trimmed();
    if (!f.isEmpty())
        sql += QStringLiteral("AND p.nome LIKE :like ");
    sql += QStringLiteral("ORDER BY p.nome COLLATE NOCASE ASC");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (!f.isEmpty())
        q.bindValue(QStringLiteral(":like"), QStringLiteral("%") + f + QStringLiteral("%"));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return itens;
    }
    while (q.next()) {
        ItemEstoque it;
        it.produtoId = q.value(0).toInt();
        it.nome = q.value(1).toString();
        it.localizacao = q.value(2).toString();
        it.unidadeBase = q.value(3).toString();
        it.minimo = q.value(4).toInt();
        it.quantidade = q.value(5).toLongLong();
        it.custoMedio = q.value(6).toLongLong() / 1000; // milésimos -> centavos
        it.temFoto = q.value(7).toInt() != 0;
        itens.push_back(it);
    }
    return itens;
}

ItemEstoque EstoqueRepository::item(int produtoId)
{
    ItemEstoque it;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT p.id, p.nome, p.localizacao, p.unidade_base, p.estoque_minimo, "
        "       COALESCE(e.quantidade_atual, 0), COALESCE(e.custo_medio_unitario, 0) "
        "FROM produtos p LEFT JOIN estoque e ON e.produto_id = p.id "
        "WHERE p.id = :id"));
    q.bindValue(QStringLiteral(":id"), produtoId);
    if (q.exec() && q.next()) {
        it.produtoId = q.value(0).toInt();
        it.nome = q.value(1).toString();
        it.localizacao = q.value(2).toString();
        it.unidadeBase = q.value(3).toString();
        it.minimo = q.value(4).toInt();
        it.quantidade = q.value(5).toLongLong();
        it.custoMedio = q.value(6).toLongLong() / 1000; // milésimos -> centavos
    }
    return it;
}

bool EstoqueRepository::garantirLinhaEstoque(int produtoId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO estoque (produto_id, quantidade_atual, custo_medio_unitario) "
        "VALUES (:pid, 0, 0)"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

bool EstoqueRepository::registrarEntrada(int produtoId, qint64 qtdBase,
                                         qint64 custoUnitBaseCentavos, int usuarioId,
                                         const QString &observacao)
{
    // Converte centavos -> milésimos (mantendo o -1 de "não mexer no custo").
    const qint64 milli = custoUnitBaseCentavos < 0 ? -1 : custoUnitBaseCentavos * 1000;
    return registrarEntradaMilli(produtoId, qtdBase, milli, usuarioId, observacao);
}

bool EstoqueRepository::registrarEntradaMilli(int produtoId, qint64 qtdBase,
                                              qint64 custoUnitBaseMilli, int usuarioId,
                                              const QString &observacao)
{
    if (qtdBase <= 0) {
        m_erro = QStringLiteral("A quantidade de entrada deve ser maior que zero.");
        return false;
    }
    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }
    if (!aplicarEntrada(produtoId, qtdBase, custoUnitBaseMilli, usuarioId,
                        QStringLiteral("entrada_manual"), observacao)) {
        m_db.rollback();
        return false;
    }
    if (!m_db.commit()) {
        m_erro = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

bool EstoqueRepository::aplicarEntrada(int produtoId, qint64 qtdBase,
                                       qint64 custoUnitBase, int usuarioId,
                                       const QString &origem, const QString &observacao)
{
    if (!garantirLinhaEstoque(produtoId))
        return false;

    qint64 qtdAtual = 0;
    qint64 custoAtual = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT quantidade_atual, custo_medio_unitario FROM estoque WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec() || !q.next()) {
            m_erro = q.lastError().text();
            return false;
        }
        qtdAtual = q.value(0).toLongLong();
        custoAtual = q.value(1).toLongLong();
    }

    const qint64 novaQtd = qtdAtual + qtdBase;
    qint64 novoCusto = custoAtual;
    if (custoUnitBase >= 0 && novaQtd > 0) {
        // Custo médio ponderado, em MILÉSIMOS de centavo (custoUnitBase e
        // custoAtual já estão nessa escala) — preserva frações de centavo.
        novoCusto = (qtdAtual * custoAtual + qtdBase * custoUnitBase) / novaQtd;
    }

    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE estoque SET quantidade_atual = :q, custo_medio_unitario = :c "
            "WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":q"), novaQtd);
        q.bindValue(QStringLiteral(":c"), novoCusto);
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            return false;
        }
    }
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO movimentacoes_estoque "
            "(produto_id, tipo, quantidade, origem, usuario_id, observacao, data) "
            "VALUES (:pid, 'entrada', :q, :origem, :uid, :obs, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        q.bindValue(QStringLiteral(":q"), qtdBase);
        q.bindValue(QStringLiteral(":origem"), origem);
        q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        q.bindValue(QStringLiteral(":obs"), observacao);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            return false;
        }
    }
    return true;
}

bool EstoqueRepository::registrarSaida(int produtoId, qint64 qtdBase,
                                       const QString &motivo, int usuarioId)
{
    if (qtdBase <= 0) {
        m_erro = QStringLiteral("A quantidade da retirada deve ser maior que zero.");
        return false;
    }
    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }
    if (!garantirLinhaEstoque(produtoId)) {
        m_db.rollback();
        return false;
    }

    qint64 atual = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT quantidade_atual FROM estoque WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec() || !q.next()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
        atual = q.value(0).toLongLong();
    }
    if (qtdBase > atual) {
        m_erro = QStringLiteral("Não há essa quantidade em estoque (disponível: %1).").arg(atual);
        m_db.rollback();
        return false;
    }

    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE estoque SET quantidade_atual = quantidade_atual - :qtd WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":qtd"), qtdBase);
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO movimentacoes_estoque "
            "(produto_id, tipo, quantidade, origem, usuario_id, observacao, data) "
            "VALUES (:pid, 'ajuste', :qtd, 'retirada', :uid, :obs, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        q.bindValue(QStringLiteral(":qtd"), -qtdBase); // saída = negativo
        q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        q.bindValue(QStringLiteral(":obs"), motivo);
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
    return true;
}

bool EstoqueRepository::registrarInventario(int produtoId, qint64 novaQtdBase,
                                            const QString &motivo, int usuarioId)
{
    if (novaQtdBase < 0) {
        m_erro = QStringLiteral("A contagem não pode ser negativa.");
        return false;
    }
    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }
    if (!garantirLinhaEstoque(produtoId)) {
        m_db.rollback();
        return false;
    }

    qint64 qtdAtual = 0;
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "SELECT quantidade_atual FROM estoque WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec() || !q.next()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
        qtdAtual = q.value(0).toLongLong();
    }

    const qint64 delta = novaQtdBase - qtdAtual;

    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "UPDATE estoque SET quantidade_atual = :q WHERE produto_id = :pid"));
        q.bindValue(QStringLiteral(":q"), novaQtdBase);
        q.bindValue(QStringLiteral(":pid"), produtoId);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }
    {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO movimentacoes_estoque "
            "(produto_id, tipo, quantidade, origem, usuario_id, observacao, data) "
            "VALUES (:pid, 'inventario', :q, 'inventario', :uid, :obs, datetime('now','localtime'))"));
        q.bindValue(QStringLiteral(":pid"), produtoId);
        q.bindValue(QStringLiteral(":q"), delta);
        q.bindValue(QStringLiteral(":uid"), usuarioId > 0 ? QVariant(usuarioId) : QVariant());
        q.bindValue(QStringLiteral(":obs"), motivo);
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
    return true;
}
