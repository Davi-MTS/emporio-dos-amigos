#pragma once

#include <QString>

struct Usuario
{
    int id = 0;
    int perfilId = 0;
    QString nome;
    QString login;
    bool ativo = true;

    // Preenchidos por join com perfis:
    QString perfilNome;
    QString permissoesJson;   // JSON cru das permissões do perfil
};
