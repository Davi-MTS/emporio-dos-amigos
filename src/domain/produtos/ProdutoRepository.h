#pragma once

#include "domain/produtos/Produto.h"

#include <QSqlDatabase>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QPair>
#include <optional>

// Acesso a dados de produtos e embalagens (CRUD sobre SQLite).
// Testável sem UI (ver tests/cpp/tst_produto_repository.cpp).
class ProdutoRepository
{
public:
    explicit ProdutoRepository(QSqlDatabase db);

    // Lista produtos ativos. `filtro` casa por nome OU código de barras.
    // Preenche categoriaNome, quantidadeEstoque e precoPrincipal (não carrega
    // a lista completa de embalagens — use obter() para isso).
    QVector<Produto> listar(const QString &filtro = QString());

    // Produto completo (com embalagens). id inexistente -> nullopt.
    std::optional<Produto> obter(int id);

    // Insere (id == 0) ou atualiza. Em sucesso, atualiza produto.id e os ids
    // das embalagens. Cria a linha de estoque zerada para produtos novos.
    // Tudo numa transação. Retorna false em erro (ver ultimoErro()).
    bool salvar(Produto &produto);

    // Soft delete (produtos.ativo = 0). Nunca remove o registro.
    bool inativar(int id);

    // Busca para o PDV: dado um código de barras, retorna o produto e a
    // embalagem correspondente (para saber o fator e o preço). nullopt se não achar.
    std::optional<QPair<Produto, Embalagem>> buscarPorCodigoBarras(const QString &codigo);

    QVector<QPair<int, QString>> listarCategorias();

    // Cria uma categoria e devolve o id. Se já existir uma com o mesmo nome
    // (sem diferenciar maiúsculas), devolve o id da existente em vez de falhar:
    // no balcão, "Cerveja" e "cerveja" são a mesma coisa.
    int criarCategoria(const QString &nome);

    // Foto do produto (JPEG já reduzido). Guardada no próprio banco para
    // acompanhar o backup — ver 0014_produto_foto.sql.
    bool salvarFoto(int produtoId, const QByteArray &jpeg);
    QByteArray foto(int produtoId);
    bool removerFoto(int produtoId);

    // Produtos (não compostos, ativos) de uma categoria — para o cliente escolher
    // o insumo específico na hora da venda.
    QVector<QPair<int, QString>> produtosDaCategoria(int categoriaId);

    // Candidatos a insumo de uma categoria, com o preço por UNIDADE BASE — é
    // essa escala que permite comparar uma lata com uma garrafa de 750 ml.
    QVector<CandidatoInsumo> candidatosDaCategoria(int categoriaId);
    double precoPorUnidadeBase(int produtoId);

    QString ultimoErro() const { return m_erro; }

private:
    QVector<Embalagem> carregarEmbalagens(int produtoId);
    bool salvarEmbalagens(Produto &produto);
    QVector<Componente> carregarComposicao(int produtoId);
    bool salvarComposicao(Produto &produto);

    QSqlDatabase m_db;
    QString m_erro;
};
