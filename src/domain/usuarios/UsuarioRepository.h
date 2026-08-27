#pragma once

#include "domain/usuarios/Usuario.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QPair>
#include <optional>

// Autenticação e CRUD de usuários. Senhas via AuthService (PBKDF2).
class UsuarioRepository
{
public:
    explicit UsuarioRepository(QSqlDatabase db);

    // Retorna o usuário se login existir, estiver ativo e a senha conferir.
    std::optional<Usuario> autenticar(const QString &login, const QString &senha);

    QVector<Usuario> listar();
    std::optional<Usuario> obter(int id);

    // Insere (id==0) ou atualiza. `senhaNova` vazia mantém a senha atual (só faz
    // sentido em update). Login deve ser único.
    bool salvar(Usuario &usuario, const QString &senhaNova);
    bool inativar(int id);

    // Quantos usuários já têm senha definida (para detectar 1º uso).
    int contarComSenha();
    // Cria/define o primeiro administrador (reaproveita linha existente sem senha).
    bool criarPrimeiroAdmin(const QString &nome, const QString &login, const QString &senha);

    QVector<QPair<int, QString>> listarPerfis();

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    QString m_erro;
};
