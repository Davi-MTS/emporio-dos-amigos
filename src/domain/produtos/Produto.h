#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

// Tipos de valor do domínio de produtos. Sem dependência de banco ou UI.
// Dinheiro sempre em centavos (qint64). Quantidades em unidade base (inteiro).

// Um nível de embalagem de um produto (unidade, caixa, fardo, pallet...).
struct Embalagem
{
    int id = 0;                 // 0 = ainda não persistida
    int produtoId = 0;
    QString nome;               // nome_embalagem, ex.: "Caixa"
    int fator = 1;              // fator_conversao (quantas unidades base)
    QString codigoBarras;
    qint64 precoVenda = 0;      // centavos
    qint64 custoCompra = -1;    // centavos; -1 = não informado
};

// Uma linha da receita de um produto composto (copão, drink, shot, dose...).
// Refere uma CATEGORIA (ex.: Destilados, Gelo) + unidade + quantidade. O produto
// específico é escolhido na hora da venda quando a categoria tem variações.
struct Componente
{
    int categoriaId = 0;
    QString categoriaNome;      // preenchido em consultas (para exibição)
    QString unidade = QStringLiteral("unidade");  // ml | unidade | g | litro...
    int quantidade = 1;         // por 1 unidade do composto (ex.: 50 = 50 ml)
};

struct Produto
{
    int id = 0;                 // 0 = novo
    QString nome;
    int categoriaId = 0;        // 0 = sem categoria
    int marcaId = 0;
    int fornecedorId = 0;
    QString unidadeBase = QStringLiteral("unidade");
    int estoqueMinimo = 0;      // unidade base
    QString localizacao;
    qint64 taxaManutencao = 0;  // centavos
    bool ativo = true;
    bool composto = false;      // "copão": vende baixando os insumos da composição

    // Dose: produto que sai de outro (a garrafa). Não tem estoque próprio —
    // o disponível vem da origem e a venda baixa a origem.
    int doseDeProdutoId = 0;    // 0 = não é dose
    qint64 doseQuantidade = 0;  // quanto consome, na unidade base da origem

    QVector<Embalagem> embalagens;
    QVector<Componente> composicao;   // insumos (só quando composto)

    // Campos derivados/juntados (preenchidos em consultas de listagem):
    QString categoriaNome;
    QString doseOrigemNome;         // nome da garrafa, para listar
    QString doseOrigemUnidade;      // unidade base da garrafa (ml, litro…)
    qint64 quantidadeEstoque = 0;   // unidade base
    qint64 precoPrincipal = 0;      // centavos: preço da embalagem de menor fator
};
