#pragma once

#include <QString>
#include <QtGlobal>

struct Cliente
{
    int id = 0;
    QString nome;
    QString telefone;
    QString cpf;
    QString endereco;
    QString aniversario;   // ISO date
    QString observacoes;
    qint64 limiteFiado = 0;   // centavos
    bool ativo = true;

    // Derivado:
    qint64 saldoDevedor = 0;  // centavos (contas a receber abertas)
};
