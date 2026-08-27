#include "database/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

Database::~Database()
{
    close();
}

bool Database::open(const QString &caminhoArquivo)
{
    close();

    // Garante que o diretório do arquivo exista.
    const QFileInfo info(caminhoArquivo);
    const QDir dir = info.absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("Não foi possível criar o diretório do banco: %1")
                          .arg(dir.absolutePath());
        return false;
    }

    QSqlDatabase database = QSqlDatabase::contains(m_connectionName)
                                ? QSqlDatabase::database(m_connectionName, /*open=*/false)
                                : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                            m_connectionName);
    database.setDatabaseName(caminhoArquivo);

    if (!database.open()) {
        m_lastError = database.lastError().text();
        return false;
    }

    // PRAGMAs de conexão. foreign_keys precisa ser ligado a cada conexão.
    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON;"),
        QStringLiteral("PRAGMA journal_mode = WAL;"),   // melhor concorrência leitura/escrita
        QStringLiteral("PRAGMA synchronous = NORMAL;"), // seguro o suficiente com WAL
        QStringLiteral("PRAGMA busy_timeout = 5000;"),  // espera locks até 5s
    };
    for (const QString &pragma : pragmas) {
        QSqlQuery query(database);
        if (!query.exec(pragma)) {
            m_lastError = QStringLiteral("Falha ao aplicar PRAGMA (%1): %2")
                              .arg(pragma, query.lastError().text());
            database.close();
            return false;
        }
    }

    m_open = true;
    m_lastError.clear();
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, /*open=*/false);
            if (database.isOpen())
                database.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_open = false;
}

bool Database::isOpen() const
{
    return m_open && QSqlDatabase::contains(m_connectionName)
           && QSqlDatabase::database(m_connectionName, /*open=*/false).isOpen();
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName, /*open=*/false);
}

QString Database::lastError() const
{
    return m_lastError;
}

QString Database::defaultDatabasePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.distribuidora");
    return QDir(base).filePath(QStringLiteral("distribuidora.db"));
}
