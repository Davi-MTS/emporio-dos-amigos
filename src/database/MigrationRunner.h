#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

// Aplica migrations de schema versionadas e scripts de seed.
//
// As migrations vivem em :/db/migrations/*.sql (embutidas como recursos) e são
// aplicadas em ordem alfabética do nome do arquivo (0001_, 0002_, ...). Cada
// migration roda numa transação e é registrada em `schema_migrations`, para não
// ser reaplicada. Nunca altere um arquivo de migration já aplicado em produção:
// crie um novo.
class MigrationRunner
{
public:
    explicit MigrationRunner(QSqlDatabase db);

    // Aplica todas as migrations pendentes. Retorna false no primeiro erro
    // (a migration que falhou sofre rollback).
    bool migrate();

    // Executa os scripts de seed (:/db/seed/*.sql). Devem ser idempotentes.
    bool seed();

    QString lastError() const;
    QStringList appliedInLastRun() const;

private:
    bool ensureMigrationsTable();
    QStringList appliedVersions();
    static QStringList scriptsIn(const QString &resourceDir);
    bool executeScript(const QString &resourcePath);

    // Divide um script SQL em statements individuais, respeitando strings
    // entre aspas simples e comentários de linha (--).
    static QStringList splitStatements(const QString &sql);

    QSqlDatabase m_db;
    QString m_lastError;
    QStringList m_applied;
};
