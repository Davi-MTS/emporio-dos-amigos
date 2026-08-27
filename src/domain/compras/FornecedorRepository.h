#pragma once

#include "domain/compras/Fornecedor.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <optional>

// CRUD de fornecedores (sem exclusão — FKs usam ON DELETE SET NULL).
class FornecedorRepository
{
public:
    explicit FornecedorRepository(QSqlDatabase db);

    QVector<Fornecedor> listar(const QString &filtro = QString());
    std::optional<Fornecedor> obter(int id);
    bool salvar(Fornecedor &fornecedor);   // insere (id==0) ou atualiza

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
