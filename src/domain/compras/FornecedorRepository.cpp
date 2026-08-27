#include "domain/compras/FornecedorRepository.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

FornecedorRepository::FornecedorRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Fornecedor> FornecedorRepository::listar(const QString &filtro)
{
    QVector<Fornecedor> lista;
    QString sql = QStringLiteral(
        "SELECT id, nome, cnpj, contato, telefone, email, endereco FROM fornecedores ");
    const QString f = filtro.trimmed();
    if (!f.isEmpty())
        sql += QStringLiteral("WHERE nome LIKE :like OR cnpj LIKE :like ");
    sql += QStringLiteral("ORDER BY nome COLLATE NOCASE");

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (!f.isEmpty())
        q.bindValue(QStringLiteral(":like"), QStringLiteral("%") + f + QStringLiteral("%"));
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Fornecedor fo;
        fo.id = q.value(0).toInt();
        fo.nome = q.value(1).toString();
        fo.cnpj = q.value(2).toString();
        fo.contato = q.value(3).toString();
        fo.telefone = q.value(4).toString();
        fo.email = q.value(5).toString();
        fo.endereco = q.value(6).toString();
        lista.push_back(fo);
    }
    return lista;
}

std::optional<Fornecedor> FornecedorRepository::obter(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, nome, cnpj, contato, telefone, email, endereco "
        "FROM fornecedores WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    Fornecedor fo;
    fo.id = q.value(0).toInt();
    fo.nome = q.value(1).toString();
    fo.cnpj = q.value(2).toString();
    fo.contato = q.value(3).toString();
    fo.telefone = q.value(4).toString();
    fo.email = q.value(5).toString();
    fo.endereco = q.value(6).toString();
    return fo;
}

bool FornecedorRepository::salvar(Fornecedor &fornecedor)
{
    if (fornecedor.nome.trimmed().isEmpty()) {
        m_erro = QStringLiteral("O nome do fornecedor é obrigatório.");
        return false;
    }
    QSqlQuery q(m_db);
    if (fornecedor.id == 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO fornecedores (nome, cnpj, contato, telefone, email, endereco) "
            "VALUES (:nome, :cnpj, :contato, :tel, :email, :end)"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE fornecedores SET nome=:nome, cnpj=:cnpj, contato=:contato, "
            "telefone=:tel, email=:email, endereco=:end WHERE id=:id"));
        q.bindValue(QStringLiteral(":id"), fornecedor.id);
    }
    q.bindValue(QStringLiteral(":nome"), fornecedor.nome.trimmed());
    q.bindValue(QStringLiteral(":cnpj"), fornecedor.cnpj);
    q.bindValue(QStringLiteral(":contato"), fornecedor.contato);
    q.bindValue(QStringLiteral(":tel"), fornecedor.telefone);
    q.bindValue(QStringLiteral(":email"), fornecedor.email);
    q.bindValue(QStringLiteral(":end"), fornecedor.endereco);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    if (fornecedor.id == 0)
        fornecedor.id = q.lastInsertId().toInt();
    return true;
}
