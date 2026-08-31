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

QVector<Produto> ProdutoRepository::listar(const QString &filtro, bool apenasSemFoto)
{
    QVector<Produto> produtos;

    QSqlQuery q(m_db);
    QString sql = QStringLiteral(
        "SELECT p.id, p.nome, p.unidade_base, p.estoque_minimo, p.ativo, "
        "       p.categoria_id, c.nome AS categoria, "
        "       COALESCE(e.quantidade_atual, 0) AS qtd, "
        "       (SELECT pe.preco_venda FROM produto_embalagens pe "
        "         WHERE pe.produto_id = p.id ORDER BY pe.fator_conversao ASC LIMIT 1) AS preco, "
        "       p.composto, "
        "       COALESCE(p.dose_de_produto_id, 0), COALESCE(p.dose_quantidade, 0), "
        "       COALESCE(o.nome, \'\'), COALESCE(o.unidade_base, \'\'), "
        // A coluna nunca era selecionada: o codigo lia value(14) numa consulta
        // de 14 colunas (0..13) -- sempre invalido, sempre false. Com isso a
        // miniatura nao aparecia em NENHUM lugar que usa listar() (lista de
        // produtos, sugestoes do PDV, carrinho); so dentro do editor, que
        // pergunta ao banco por outro caminho.
        "       (p.foto IS NOT NULL AND length(p.foto) > 0) AS tem_foto "
        "FROM produtos p "
        "LEFT JOIN categorias c ON c.id = p.categoria_id "
        "LEFT JOIN estoque e ON e.produto_id = p.id "
        "LEFT JOIN produtos o ON o.id = p.dose_de_produto_id "
        "WHERE p.ativo = 1 ");

    if (apenasSemFoto)
        sql += QStringLiteral("AND (p.foto IS NULL OR length(p.foto) = 0) ");

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
        p.doseDeProdutoId = q.value(10).toInt();
        p.doseQuantidade = q.value(11).toLongLong();
        p.doseOrigemNome = q.value(12).toString();
        p.doseOrigemUnidade = q.value(13).toString();
        p.temFoto = q.value(14).toInt() != 0;
        produtos.push_back(p);
    }
    return produtos;
}

int ProdutoRepository::contarSemFoto()
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM produtos "
                               "WHERE ativo = 1 AND (foto IS NULL OR length(foto) = 0)"))
        || !q.next()) {
        m_erro = q.lastError().text();
        return 0;
    }
    return q.value(0).toInt();
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
        "       p.ativo, c.nome, COALESCE(e.quantidade_atual, 0), p.composto, "
        "       COALESCE(p.dose_de_produto_id, 0), COALESCE(p.dose_quantidade, 0), "
        "       COALESCE(o.nome, \'\'), COALESCE(o.unidade_base, \'\'), "
        // Mesmo defeito do listar(): lia value(17) numa consulta de 17 colunas.
        "       (p.foto IS NOT NULL AND length(p.foto) > 0) AS tem_foto "
        "FROM produtos p "
        "LEFT JOIN categorias c ON c.id = p.categoria_id "
        "LEFT JOIN estoque e ON e.produto_id = p.id "
        "LEFT JOIN produtos o ON o.id = p.dose_de_produto_id "
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
    p.doseDeProdutoId = q.value(13).toInt();
    p.doseQuantidade = q.value(14).toLongLong();
    p.doseOrigemNome = q.value(15).toString();
    p.doseOrigemUnidade = q.value(16).toString();
    p.temFoto = q.value(17).toInt() != 0;
    p.embalagens = carregarEmbalagens(p.id);
    p.composicao = carregarComposicao(p.id);
    return p;
}

QVector<Componente> ProdutoRepository::carregarComposicao(int produtoId)
{
    QVector<Componente> lista;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT pc.categoria_id, pc.quantidade, pc.unidade, c.nome, "
        "       COALESCE(pc.produto_padrao_id, 0), COALESCE(pp.nome, ''), "
        "       COALESCE(pc.travada, 0) "
        "FROM produto_composicao pc "
        "JOIN categorias c ON c.id = pc.categoria_id "
        "LEFT JOIN produtos pp ON pp.id = pc.produto_padrao_id "
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
        c.produtoPadraoId = q.value(4).toInt();
        c.produtoPadraoNome = q.value(5).toString();
        c.travada = q.value(6).toInt() != 0;
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
            "INSERT INTO produto_composicao (produto_composto_id, categoria_id, unidade, "
            " quantidade, produto_padrao_id, travada) "
            "VALUES (:comp, :cat, :un, :qtd, :padrao, :travada)"));
        q.bindValue(QStringLiteral(":comp"), produto.id);
        q.bindValue(QStringLiteral(":cat"), c.categoriaId);
        q.bindValue(QStringLiteral(":un"), c.unidade);
        q.bindValue(QStringLiteral(":qtd"), c.quantidade);
        q.bindValue(QStringLiteral(":padrao"),
                    c.produtoPadraoId > 0 ? QVariant(c.produtoPadraoId) : QVariant());
        q.bindValue(QStringLiteral(":travada"), c.travada ? 1 : 0);
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
    for (const CandidatoInsumo &c : candidatosDaCategoria(categoriaId))
        lista.push_back({c.id, c.nome});
    return lista;
}

QVector<CandidatoInsumo> ProdutoRepository::candidatosDaCategoria(int categoriaId)
{
    QVector<CandidatoInsumo> lista;
    QSqlQuery q(m_db);
    // O preço por UNIDADE BASE é o que permite comparar coisas diferentes: uma
    // lata de energético (fator 1) com uma garrafa de 750 ml. Sem dividir pelo
    // fator, trocar o destilado somaria o preço da garrafa inteira no copão.
    q.prepare(QStringLiteral(
        "SELECT p.id, p.nome, "
        "       (SELECT CAST(pe.preco_venda AS REAL) / MAX(pe.fator_conversao, 1) "
        "          FROM produto_embalagens pe WHERE pe.produto_id = p.id "
        "         ORDER BY pe.fator_conversao ASC LIMIT 1) "
        "FROM produtos p "
        "WHERE p.ativo = 1 AND p.composto = 0 AND COALESCE(p.dose_de_produto_id,0) = 0 "
        "  AND p.categoria_id = :cat "
        "ORDER BY p.nome COLLATE NOCASE"));
    q.bindValue(QStringLiteral(":cat"), categoriaId);
    if (q.exec()) {
        while (q.next()) {
            CandidatoInsumo c;
            c.id = q.value(0).toInt();
            c.nome = q.value(1).toString();
            c.precoPorUnidadeBase = q.value(2).toDouble();
            lista.push_back(c);
        }
    }
    return lista;
}

double ProdutoRepository::precoPorUnidadeBase(int produtoId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT CAST(preco_venda AS REAL) / MAX(fator_conversao, 1) "
        "FROM produto_embalagens WHERE produto_id = :pid "
        "ORDER BY fator_conversao ASC LIMIT 1"));
    q.bindValue(QStringLiteral(":pid"), produtoId);
    if (q.exec() && q.next())
        return q.value(0).toDouble();
    return 0.0;
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
            " estoque_minimo, localizacao, taxa_manutencao, ativo, composto, "
            " dose_de_produto_id, dose_quantidade) "
            "VALUES (:nome, :cat, :marca, :forn, :ub, :min, :loc, :taxa, 1, :composto, "
            "        :doseOrigem, :doseQtd)"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE produtos SET nome=:nome, categoria_id=:cat, marca_id=:marca, "
            " fornecedor_id=:forn, unidade_base=:ub, estoque_minimo=:min, "
            " localizacao=:loc, taxa_manutencao=:taxa, composto=:composto, "
            " dose_de_produto_id=:doseOrigem, dose_quantidade=:doseQtd WHERE id=:id"));
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
    // Dose sem origem ou sem quantidade não é dose: grava NULL/0 e o produto
    // volta a ser normal, em vez de virar um item que não baixa nada.
    const bool ehDose = produto.doseDeProdutoId > 0 && produto.doseQuantidade > 0
                        && produto.doseDeProdutoId != produto.id;
    q.bindValue(QStringLiteral(":doseOrigem"),
                ehDose ? QVariant(produto.doseDeProdutoId) : QVariant());
    q.bindValue(QStringLiteral(":doseQtd"),
                ehDose ? static_cast<qlonglong>(produto.doseQuantidade) : Q_INT64_C(0));

    if (!q.exec()) {
        m_erro = q.lastError().text();
        m_db.rollback();
        return false;
    }

    if (produto.id == 0)
        produto.id = q.lastInsertId().toInt();

    // Composto ("copão") e dose não têm estoque próprio: o composto baixa os
    // insumos e a dose baixa a garrafa de origem. Criar linha de estoque para
    // eles daria dois saldos para a mesma mercadoria.
    if (!produto.composto && !ehDose) {
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

bool ProdutoRepository::salvarFoto(int produtoId, const QByteArray &jpeg)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE produtos SET foto = :foto WHERE id = :id"));
    q.bindValue(QStringLiteral(":foto"), jpeg.isEmpty() ? QVariant() : QVariant(jpeg));
    q.bindValue(QStringLiteral(":id"), produtoId);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

QByteArray ProdutoRepository::foto(int produtoId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT foto FROM produtos WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), produtoId);
    if (!q.exec() || !q.next()) {
        m_erro = q.lastError().text();
        return {};
    }
    return q.value(0).toByteArray();
}

bool ProdutoRepository::removerFoto(int produtoId)
{
    return salvarFoto(produtoId, QByteArray());
}
