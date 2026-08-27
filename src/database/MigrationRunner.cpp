#include "database/MigrationRunner.h"

#include <utility>

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

MigrationRunner::MigrationRunner(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QString MigrationRunner::lastError() const
{
    return m_lastError;
}

QStringList MigrationRunner::appliedInLastRun() const
{
    return m_applied;
}

bool MigrationRunner::ensureMigrationsTable()
{
    QSqlQuery query(m_db);
    const bool ok = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "  version    TEXT PRIMARY KEY,"
        "  applied_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ")"));
    if (!ok)
        m_lastError = query.lastError().text();
    return ok;
}

QStringList MigrationRunner::appliedVersions()
{
    QStringList versions;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT version FROM schema_migrations"))) {
        while (query.next())
            versions << query.value(0).toString();
    }
    return versions;
}

QStringList MigrationRunner::scriptsIn(const QString &resourceDir)
{
    QDir dir(resourceDir);
    // QDir::Name garante ordem alfabética: 0001_ antes de 0002_.
    return dir.entryList(QStringList{QStringLiteral("*.sql")}, QDir::Files, QDir::Name);
}

bool MigrationRunner::migrate()
{
    m_applied.clear();

    if (!ensureMigrationsTable())
        return false;

    const QStringList aplicadas = appliedVersions();
    const QStringList disponiveis = scriptsIn(QStringLiteral(":/db/migrations"));

    for (const QString &nome : disponiveis) {
        if (aplicadas.contains(nome))
            continue;

        if (!m_db.transaction()) {
            m_lastError = QStringLiteral("Não foi possível iniciar transação para %1: %2")
                              .arg(nome, m_db.lastError().text());
            return false;
        }

        if (!executeScript(QStringLiteral(":/db/migrations/%1").arg(nome))) {
            m_db.rollback();
            m_lastError = QStringLiteral("Migration %1 falhou: %2").arg(nome, m_lastError);
            return false;
        }

        QSqlQuery registro(m_db);
        registro.prepare(QStringLiteral(
            "INSERT INTO schema_migrations (version) VALUES (:version)"));
        registro.bindValue(QStringLiteral(":version"), nome);
        if (!registro.exec()) {
            m_lastError = registro.lastError().text();
            m_db.rollback();
            return false;
        }

        if (!m_db.commit()) {
            m_lastError = m_db.lastError().text();
            m_db.rollback();
            return false;
        }

        m_applied << nome;
    }

    return true;
}

bool MigrationRunner::seed()
{
    const QStringList scripts = scriptsIn(QStringLiteral(":/db/seed"));
    for (const QString &nome : scripts) {
        if (!executeScript(QStringLiteral(":/db/seed/%1").arg(nome))) {
            m_lastError = QStringLiteral("Seed %1 falhou: %2").arg(nome, m_lastError);
            return false;
        }
    }
    return true;
}

bool MigrationRunner::executeScript(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Não foi possível ler o script %1").arg(resourcePath);
        return false;
    }
    const QString sql = QString::fromUtf8(file.readAll());
    file.close();

    const QStringList statements = splitStatements(sql);
    for (const QString &stmt : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(stmt)) {
            m_lastError = QStringLiteral("%1\n>>> %2")
                              .arg(query.lastError().text(), stmt.left(120));
            return false;
        }
    }
    return true;
}

QStringList MigrationRunner::splitStatements(const QString &sql)
{
    QStringList statements;
    QString atual;
    bool emString = false;    // dentro de '...'
    bool emComentario = false; // dentro de -- até o fim da linha

    const int n = sql.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = sql.at(i);

        if (emComentario) {
            if (c == QLatin1Char('\n')) {
                emComentario = false;
                atual.append(c);
            }
            continue;
        }

        if (emString) {
            atual.append(c);
            if (c == QLatin1Char('\'')) {
                // '' escapa aspa simples dentro da string.
                if (i + 1 < n && sql.at(i + 1) == QLatin1Char('\'')) {
                    atual.append(sql.at(++i));
                } else {
                    emString = false;
                }
            }
            continue;
        }

        // Fora de string e de comentário.
        if (c == QLatin1Char('-') && i + 1 < n && sql.at(i + 1) == QLatin1Char('-')) {
            emComentario = true;
            ++i; // consome o segundo '-'
            continue;
        }
        if (c == QLatin1Char('\'')) {
            emString = true;
            atual.append(c);
            continue;
        }
        if (c == QLatin1Char(';')) {
            if (!atual.trimmed().isEmpty())
                statements << atual.trimmed();
            atual.clear();
            continue;
        }
        atual.append(c);
    }

    if (!atual.trimmed().isEmpty())
        statements << atual.trimmed();

    return statements;
}
