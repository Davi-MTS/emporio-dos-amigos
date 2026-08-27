#pragma once

#include <QSqlDatabase>
#include <QString>

// Gerencia a conexão SQLite única do aplicativo.
//
// Responsável por abrir/criar o arquivo do banco e configurar os PRAGMAs
// obrigatórios (foreign_keys, WAL, busy_timeout). A regra de negócio recebe a
// QSqlDatabase por `connection()` — nunca abre conexões por conta própria.
class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Abre (ou cria) o banco no caminho informado e aplica os PRAGMAs.
    // Retorna false em caso de falha; detalhe em lastError().
    bool open(const QString &caminhoArquivo);
    void close();
    bool isOpen() const;

    QSqlDatabase connection() const;
    QString lastError() const;

    // Caminho padrão do banco: <AppDataLocation>/distribuidora.db.
    // Cria o diretório se necessário.
    static QString defaultDatabasePath();

private:
    QString m_connectionName{QStringLiteral("distribuidora_main")};
    QString m_lastError;
    bool m_open{false};
};
