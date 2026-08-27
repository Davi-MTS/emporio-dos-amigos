#include "domain/usuarios/UsuarioRepository.h"

#include "services/auth/AuthService.h"

#include <utility>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

UsuarioRepository::UsuarioRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

std::optional<Usuario> UsuarioRepository::autenticar(const QString &login,
                                                     const QString &senha)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT u.id, u.perfil_id, u.nome, u.login, u.ativo, u.senha_hash, "
        "       p.nome, p.permissoes "
        "FROM usuarios u JOIN perfis p ON p.id = u.perfil_id "
        "WHERE u.login = :login AND u.ativo = 1"));
    q.bindValue(QStringLiteral(":login"), login.trimmed());
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next())
        return std::nullopt;

    const QString hash = q.value(5).toString();
    if (!AuthService::verificar(senha, hash))
        return std::nullopt;

    Usuario u;
    u.id = q.value(0).toInt();
    u.perfilId = q.value(1).toInt();
    u.nome = q.value(2).toString();
    u.login = q.value(3).toString();
    u.ativo = q.value(4).toInt() != 0;
    u.perfilNome = q.value(6).toString();
    u.permissoesJson = q.value(7).toString();
    return u;
}

QVector<Usuario> UsuarioRepository::listar()
{
    QVector<Usuario> lista;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT u.id, u.perfil_id, u.nome, u.login, u.ativo, p.nome "
            "FROM usuarios u JOIN perfis p ON p.id = u.perfil_id "
            "WHERE u.ativo = 1 ORDER BY u.nome COLLATE NOCASE"))) {
        m_erro = q.lastError().text();
        return lista;
    }
    while (q.next()) {
        Usuario u;
        u.id = q.value(0).toInt();
        u.perfilId = q.value(1).toInt();
        u.nome = q.value(2).toString();
        u.login = q.value(3).toString();
        u.ativo = q.value(4).toInt() != 0;
        u.perfilNome = q.value(5).toString();
        lista.push_back(u);
    }
    return lista;
}

std::optional<Usuario> UsuarioRepository::obter(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT u.id, u.perfil_id, u.nome, u.login, u.ativo, p.nome "
        "FROM usuarios u JOIN perfis p ON p.id = u.perfil_id WHERE u.id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    Usuario u;
    u.id = q.value(0).toInt();
    u.perfilId = q.value(1).toInt();
    u.nome = q.value(2).toString();
    u.login = q.value(3).toString();
    u.ativo = q.value(4).toInt() != 0;
    u.perfilNome = q.value(5).toString();
    return u;
}

bool UsuarioRepository::salvar(Usuario &usuario, const QString &senhaNova)
{
    if (usuario.nome.trimmed().isEmpty() || usuario.login.trimmed().isEmpty()) {
        m_erro = QStringLiteral("Nome e login são obrigatórios.");
        return false;
    }
    if (usuario.id == 0 && senhaNova.isEmpty()) {
        m_erro = QStringLiteral("Defina uma senha para o novo usuário.");
        return false;
    }

    QSqlQuery q(m_db);
    if (usuario.id == 0) {
        q.prepare(QStringLiteral(
            "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) "
            "VALUES (:perfil, :nome, :login, :hash, 1)"));
        q.bindValue(QStringLiteral(":hash"), AuthService::hashSenha(senhaNova));
    } else if (!senhaNova.isEmpty()) {
        q.prepare(QStringLiteral(
            "UPDATE usuarios SET perfil_id=:perfil, nome=:nome, login=:login, "
            "senha_hash=:hash WHERE id=:id"));
        q.bindValue(QStringLiteral(":hash"), AuthService::hashSenha(senhaNova));
        q.bindValue(QStringLiteral(":id"), usuario.id);
    } else {
        q.prepare(QStringLiteral(
            "UPDATE usuarios SET perfil_id=:perfil, nome=:nome, login=:login WHERE id=:id"));
        q.bindValue(QStringLiteral(":id"), usuario.id);
    }
    q.bindValue(QStringLiteral(":perfil"), usuario.perfilId);
    q.bindValue(QStringLiteral(":nome"), usuario.nome.trimmed());
    q.bindValue(QStringLiteral(":login"), usuario.login.trimmed());
    if (!q.exec()) {
        m_erro = q.lastError().text().contains(QStringLiteral("UNIQUE"))
                     ? QStringLiteral("Já existe um usuário com esse login.")
                     : q.lastError().text();
        return false;
    }
    if (usuario.id == 0)
        usuario.id = q.lastInsertId().toInt();
    return true;
}

bool UsuarioRepository::inativar(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE usuarios SET ativo = 0 WHERE id = :id"));
    q.bindValue(QStringLiteral(":id"), id);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

int UsuarioRepository::contarComSenha()
{
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM usuarios WHERE senha_hash <> ''"))
        && q.next())
        return q.value(0).toInt();
    return 0;
}

bool UsuarioRepository::criarPrimeiroAdmin(const QString &nome, const QString &login,
                                           const QString &senha)
{
    if (nome.trimmed().isEmpty() || login.trimmed().isEmpty() || senha.isEmpty()) {
        m_erro = QStringLiteral("Preencha nome, login e senha.");
        return false;
    }
    const QString hash = AuthService::hashSenha(senha);

    // Reaproveita uma linha existente sem senha (placeholder), se houver.
    int existente = 0;
    {
        QSqlQuery q(m_db);
        if (q.exec(QStringLiteral("SELECT id FROM usuarios ORDER BY id LIMIT 1")) && q.next())
            existente = q.value(0).toInt();
    }

    QSqlQuery q(m_db);
    if (existente > 0) {
        q.prepare(QStringLiteral(
            "UPDATE usuarios SET perfil_id=1, nome=:nome, login=:login, "
            "senha_hash=:hash, ativo=1 WHERE id=:id"));
        q.bindValue(QStringLiteral(":id"), existente);
    } else {
        q.prepare(QStringLiteral(
            "INSERT INTO usuarios (perfil_id, nome, login, senha_hash, ativo) "
            "VALUES (1, :nome, :login, :hash, 1)"));
    }
    q.bindValue(QStringLiteral(":nome"), nome.trimmed());
    q.bindValue(QStringLiteral(":login"), login.trimmed());
    q.bindValue(QStringLiteral(":hash"), hash);
    if (!q.exec()) {
        m_erro = q.lastError().text();
        return false;
    }
    return true;
}

QVector<QPair<int, QString>> UsuarioRepository::listarPerfis()
{
    QVector<QPair<int, QString>> lista;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT id, nome FROM perfis ORDER BY id"))) {
        while (q.next())
            lista.push_back({q.value(0).toInt(), q.value(1).toString()});
    }
    return lista;
}
