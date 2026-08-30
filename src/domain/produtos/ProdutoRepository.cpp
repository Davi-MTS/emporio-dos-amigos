#include "domain/produtos/ProdutoRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

ProdutoRepository::ProdutoRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Produto> ProdutoRepository::listar(const QString &filtro)
{
    QVector<Produto> produtos;

    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT p.id, p.nome, p.unidade_base, p.estoque_minimo, p.ativo, "
        "       p.categoria_id, c.nome AS categoria, "
        "       COALESCE(e.quantidade_atual, 0) AS qtd, "
        "       (SELECT pe.preco_venda FROM produto_embalagens pe "
        "         WHERE pe.produto_id = p.id ORDER BY pe.fator_conversao ASC LIMIT 1) AS preco, "
        "       p.composto "
        "FROM produtos p "
        "LEFT JOIN categorias c ON c.id = p.categoria_id "
        "LEFT JOIN estoque e ON e.produto_id = p.id "
        "WHERE p.ativo = 1 ");

    const QString f = filtro.trimmed();
    if (!f.isEmpty()) {
        sql += QStringLiteral(
            "AND (p.nome LIKE :like "
            "     OR EXISTS (SELECT 1 FROM produto_embalagens pe "
            "                WHERE pe.produto_id = p.id AND pe.codigo_barras = :codigo)) ");
    }
    sql += QStringLiteral("ORDER BY p.nome COLLATE NOCASE ASC");

    q.prepare(sql);
    if (!f.isEmpty()) {
        q.bindValue(QStringLiteral(":like"), QStringLiteral("%") + f + QStringLiteral("%"));
        q.bindValue(QStringLiteral(":codigo"), f);
    }

    if (!q.exec()) {
        m_erro = q.lastError().text();
        return produtos;
    }

    while (q.next()) {
        Produto p;
        p.id = q.value(0).toInt();
        p.nome = q.value(1).toString();
        p.unidadeBase = q.value(2).toString();
        p.estoqueMinimo = q.value(3).toInt();
        p.ativo = q.value(4).toInt() != 0;
        p.categoriaId = q.value(5).toInt();
        p.categoriaNome = q.value(6).toString();
        p.quantidadeEstoque = q.value(7).toLongLong();
        p.precoPrincipal = q.value(8).toLongLong();
        p.composto = q.value(9).toInt() != 0;
        produtos.push_back(p);
    }
    return produtos;
}

QVector<Embalagem> ProdutoRepository::carregarEmbalagens(int produtoId)
{
    QVector<Embalagem> lista;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, produto_id, nome_embalagem, fator_conversao, codigo_barras, "
        "       preco_venda, custo_compra "
        "FROM produto_embalagens WHERE produto_id = :pid "
        "ORDER BY fator_conversao ASC"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Embalagem e;
        e.id = q.value(0).toInt();
        e.produtoId = q.value(1).toInt();
        e.nome = q.value(2).toString();
        e.fator = q.value(3).toInt();
        e.codigoBarras = q.value(4).toString();
        e.precoVenda = q.value(5).toLongLong();
        e.custoCompra = q.value(6).isNull() ? -1 : q.value(6).toLongLong();
        lista.push_back(e);
    }
    return lista;
}

std::optional<Produto> ProdutoRepository::obter(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT p.id, p.nome, p.categoria_id, p.marca_id, p.fornecedor_id, "
        "       p.unidade_base, p.estoque_minimo, p.localizacao, p.taxa_manutencao, "
        "       p.ativo, c.nome, COALESCE(e.quantidade_atual, 0), p.composto "
        "FROM produtos p "
        "LEFT JOIN categorias c ON c.id = p.categoria_id "
        "LEFT JOIN estoque e ON e.produto_id = p.id "
        "WHERE p.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;

    Produto p;
    p.id = q.value(0).toInt();
    p.nome = q.value(1).toString();
    p.categoriaId = q.value(2).toInt();
    p.marcaId = q.value(3).toInt();
    p.fornecedorId = q.value(4).toInt();
    p.unidadeBase = q.value(5).toString();
    p.estoqueMinimo = q.value(6).toInt();
    p.localizacao = q.value(7).toString();
    p.taxaManutencao = q.value(8).toLongLong();
    p.ativo = q.value(9).toInt() != 0;
    p.categoriaNome = q.value(10).toString();
    p.quantidadeEstoque = q.value(11).toLongLong();
    p.composto = q.value(12).toInt() != 0;
    p.embalagens = carregarEmbalagens(p.id);
    p.composicao = carregarComposicao(p.id);
    return p;
}

QVector<Componente> ProdutoRepository::carregarComposicao(int produtoId)
{
    QVector<Componente> lista;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT pc.categoria_id, pc.quantidade, pc.unidade, c.nome "
        "FROM produto_composicao pc JOIN categorias c ON c.id = pc.categoria_id "
        "WHERE pc.produto_composto_id = :pid ORDER BY c.nome COLLATE NOCASE"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Componente c;
        c.categoriaId = q.value(0).toInt();
        c.quantidade = q.value(1).toInt();
        c.unidade = q.value(2).toString();
        c.categoriaNome = q.value(3).toString();
        lista.push_back(c);
    }
    return lista;
}

bool ProdutoRepository::salvarComposicao(Produto &produto)
{
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral(
        "DELETE FROM produto_composicao WHERE produto_composto_id = :pid"));
    del.bindValue(QStringLiteral(":pid"), produto.id);
    if (!del.exec()) {
        m_erro = del.lastError().text();
        return false;
    }
    if (!produto.composto)
        return true;

    for (const Componente &c : produto.composicao) {
        if (c.categoriaId <= 0 || c.quantidade <= 0)
            continue;
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO produto_composicao (produto_composto_id, categoria_id, unidade, quantidade) "
            "VALUES (:comp, :cat, :un, :qtd)"));
        q.bindValue(QStringLiteral(":comp"), produto.id);
        q.bindValue(QStringLiteral(":cat"), c.categoriaId);
        q.bindValue(QStringLiteral(":un"), c.unidade);
        q.bindValue(QStringLiteral(":qtd"), c.quantidade);
        if (!q.exec()) {
            m_erro = q.lastError().text();
            return false;
        }
    }
    return true;
}

QVector<QPair<int, QString>> ProdutoRepository::produtosDaCategoria(int categoriaId)
{
    QVector<QPair<int, QString>> lista;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, nome FROM produtos "
        "WHERE ativo = 1 AND composto = 0 AND categoria_id = :cat "
        "ORDER BY nome COLLATE NOCASE"));
    q.bindValue(QStringLiteral(":cat"), categoriaId);
    if (q.exec()) {
        while (q.next())
            lista.push_back({q.value(0).toInt(), q.value(1).toString()});
    }
    return lista;
}

bool ProdutoRepository::salvar(Produto &produto)
{
    if (produto.nome.trimmed().isEmpty()) {
        m_erro = QStringLiteral("O nome do produto é obrigatório.");
        return false;
    }

    if (!m_db.transaction()) {
        m_erro = m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    if (produto.id == 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO produtos "
            "(nome, categoria_id, marca_id, fornecedor_id, unidade_base, "
            " estoque_minimo, localizacao, taxa_manutencao, ativo, composto) "
            "VALUES (:nome, :cat, :marca, :forn, :ub, :min, :loc, :taxa, 1, :composto)"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE produtos SET nome=:nome, categoria_id=:cat, marca_id=:marca, "
            " fornecedor_id=:forn, unidade_base=:ub, estoque_minimo=:min, "
            " localizacao=:loc, taxa_manutencao=:taxa, composto=:composto WHERE id=:id"));
        q.bindValue(QStringLiteral(":id"), produto.id);
    }
    q.bindValue(QStringLiteral(":nome"), produto.nome.trimmed());
    q.bindValue(QStringLiteral(":cat"), produto.categoriaId > 0 ? QVariant(produto.categoriaId) : QVariant());
    q.bindValue(QStringLiteral(":marca"), produto.marcaId > 0 ? QVariant(produto.marcaId) : QVariant());
    q.bindValue(QStringLiteral(":forn"), produto.fornecedorId > 0 ? QVariant(produto.fornecedorId) : QVariant());
    q.bindValue(QStringLiteral(":ub"), produto.unidadeBase);
    q.bindValue(QStringLiteral(":min"), produto.estoqueMinimo);
    q.bindValue(QStringLiteral(":loc"), produto.localizacao);
    q.bindValue(QStringLiteral(":taxa"), produto.taxaManutencao);
    q.bindValue(QStringLiteral(":composto"), produto.composto ? 1 : 0);

    if (!q.exec()) {
        m_erro = q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (produto.id == 0)
        produto.id = q.lastInsertId().toInt();

    // Produto composto ("copão") não tem estoque próprio — só baixa insumos.
    if (!produto.composto) {
        QSqlQuery est(m_db);
        est.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO estoque (produto_id, quantidade_atual, custo_medio_unitario) "
            "VALUES (:pid, 0, 0)"));
        est.bindValue(QStringLiteral(":pid"), produto.id);
        if (!est.exec()) {
            m_erro = est.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!salvarEmbalagens(produto)) {
        m_db.rollback();
        return false;
    }

    if (!salvarComposicao(produto)) {
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

bool ProdutoRepository::salvarEmbalagens(Produto &produto)
{
    // Garante ao menos a embalagem base (fator 1).
    if (produto.embalagens.isEmpty()) {
        Embalagem base;
        base.nome = produto.unidadeBase;
        base.fator = 1;
        produto.embalagens.push_back(base);
    }

    // Ids que continuam existindo (para apagar os removidos).
    QVector<int> mantidos;

    for (Embalagem &e : produto.embalagens) {
        e.produtoId = produto.id;
        if (e.fator <= 0)
            e.fator = 1;

        QSqlQuery q(m_db);
        if (e.id == 0) {
            q.prepare(QStringLiteral(
                "INSERT INTO produto_embalagens "
                "(produto_id, nome_embalagem, fator_conversao, codigo_barras, preco_venda, custo_compra) "
                "VALUES (:pid, :nome, :fator, :cod, :preco, :custo)"));
        } else {
            q.prepare(QStringLiteral(
                "UPDATE produto_embalagens SET nome_embalagem=:nome, fator_conversao=:fator, "
                " codigo_barras=:cod, preco_venda=:preco, custo_compra=:custo "
                "WHERE id=:id AND produto_id=:pid"));
            q.bindValue(QStringLiteral(":id"), e.id);
        }
        q.bindValue(QStringLiteral(":pid"), produto.id);
        q.bindValue(QStringLiteral(":nome"), e.nome);
        q.bindValue(QStringLiteral(":fator"), e.fator);
        q.bindValue(QStringLiteral(":cod"),
                    e.codigoBarras.trimmed().isEmpty() ? QVariant() : QVariant(e.codigoBarras.trimmed()));
        q.bindValue(QStringLiteral(":preco"), e.precoVenda);
        q.bindValue(QStringLiteral(":custo"), e.custoCompra < 0 ? QVariant() : QVariant(e.custoCompra));

        if (!q.exec()) {
            m_erro = q.lastError().text();
            return false;
        }
        if (e.id == 0)
            e.id = q.lastInsertId().toInt();
        mantidos.push_back(e.id);
    }

    // Remove embalagens que existiam mas não vieram na lista.
    QStringList placeholders;
    for (int i = 0; i < mantidos.size(); ++i)
        placeholders << QStringLiteral(":k%1").arg(i);

    QSqlQuery del(m_db);
    del.prepare(QStringLiteral(
        "DELETE FROM produto_embalagens WHERE produto_id = :pid AND id NOT IN (%1)")
        .arg(placeholders.join(QStringLiteral(", "))));
    del.bindValue(QStringLiteral(":pid"), produto.id);
    for (int i = 0; i < mantidos.size(); ++i)
        del.bindValue(QStringLiteral(":k%1").arg(i), mantidos.at(i));
    if (!del.exec()) {
        m_erro = del.lastError().text();
        return false;
    }
    return true;
}

bool ProdutoRepository::inativar(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE produtos SET ativo = 0 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

std::optional<QPair<Produto, Embalagem>>
ProdutoRepository::buscarPorCodigoBarras(const QString &codigo)
{
    const QString cod = codigo.trimmed();
    if (cod.isEmpty())
        return std::nullopt;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT produto_id FROM produto_embalagens WHERE codigo_barras = :cod LIMIT 1"));
    q.bindValue(QStringLiteral(":cod"), cod);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;

    const int produtoId = q.value(0).toInt();
    const auto produto = obter(produtoId);
    if (!produto)
        return std::nullopt;

    for (const Embalagem &e : produto->embalagens) {
        if (e.codigoBarras == cod)
            return QPair<Produto, Embalagem>(*produto, e);
    }
    return std::nullopt;
}

QVector<QPair<int, QString>> ProdutoRepository::listarCategorias()
{
    QVector<QPair<int, QString>> lista;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id, nome FROM categorias ORDER BY nome COLLATE NOCASE"))) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next())
        lista.push_back({q.value(0).toInt(), q.value(1).toString()});
    return lista;
}

int ProdutoRepository::criarCategoria(const QString &nome)
{
    const QString limpo = nome.trimmed();
    if (limpo.isEmpty()) {
        m_erro = QStringLiteral("Informe o nome da categoria.");
        return 0;
    }

    QSqlQuery busca(m_db);
    busca.prepare(QStringLiteral(
        "SELECT id FROM categorias WHERE nome = :nome COLLATE NOCASE"));
    busca.bindValue(QStringLiteral(":nome"), limpo);
    if (busca.exec() && busca.next())
        return busca.value(0).toInt();   // já existe: reaproveita

    QSqlQuery ins(m_db);
    ins.prepare(QStringLiteral("INSERT INTO categorias (nome) VALUES (:nome)"));
    ins.bindValue(QStringLiteral(":nome"), limpo);
    if (!ins.exec()) {
        m_erro = ins.lastError().text();
        return 0;
    }
    return ins.lastInsertId().toInt();
}
