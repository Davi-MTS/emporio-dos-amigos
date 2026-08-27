#pragma once

#include "domain/compras/Compra.h"
#include "domain/estoque/EstoqueRepository.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

// Registro de compras (entrada de mercadoria). Ao registrar, dá entrada no
// estoque de cada item (atualizando o custo médio) e, opcionalmente, cria uma
// conta a pagar — tudo numa única transação.
class CompraRepository
{
public:
    explicit CompraRepository(QSqlDatabase db);

    QVector<CompraResumo> listar();

    // numeroNota/dataNota identificam a nota fiscal de origem (opcionais). Se
    // numeroNota vier preenchido, a compra é marcada com origem 'nota'.
    ResultadoCompra registrarCompra(int fornecedorId, const QString &origem,
                                    const QVector<ItemCompra> &itens,
                                    bool gerarContaPagar, const QString &vencimento,
                                    int usuarioId,
                                    const QString &numeroNota = QString(),
                                    const QString &dataNota = QString());

    QString ultimoErro() const { return m_erro; }

private:
    QSqlDatabase m_db;
    EstoqueRepository m_estoque;
    QString m_erro;
};
